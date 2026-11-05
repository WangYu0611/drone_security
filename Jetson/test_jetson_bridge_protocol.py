import ast
import contextlib
import io
import json
import math
import os
import pathlib
import socket
import sys
import threading
import time
import types
import unittest
from unittest.mock import Mock, patch


SCRIPT_PATH = pathlib.Path(__file__).with_name("jetson_bridge.py")


class _Logger:
    def info(self, _message):
        pass

    def warning(self, _message):
        pass

    def error(self, _message):
        pass


def _load_protocol_code():
    """加载纯协议代码，避免测试机必须安装 ROS2/px4_msgs。"""
    tree = ast.parse(SCRIPT_PATH.read_text(encoding="utf-8"))
    helper_names = {
        "read_arm_confirmation", "parse_speed_mps", "format_speed_mps",
        "parse_control_packet",
    }
    helpers = [node for node in tree.body
               if isinstance(node, ast.FunctionDef) and node.name in helper_names]
    bridge = next(
        node for node in tree.body
        if isinstance(node, ast.ClassDef) and node.name == "JetsonBridge"
    )
    method_names = {
        "_accept_backend_session",
        "_control_fingerprint",
        "_stage_control_command",
        "_apply_control_command",
        "_prune_applied_commands",
        "process_control",
    }
    gate_class = ast.ClassDef(
        name="ProtocolGate",
        bases=[],
        keywords=[],
        decorator_list=[],
        body=[
            node for node in bridge.body
            if isinstance(node, ast.FunctionDef) and node.name in method_names
        ],
    )
    namespace = {
        "json": json,
        "math": math,
        "time": time,
        "socket": socket,
        "sys": sys,
        "running": True,
        "MAX_CONTROL_PACKET_BYTES": 4096,
        "MAX_ABS_TARGET_M": 5000.0,
        "CONTROL_PROTOCOL": "ue5_drone_control",
        "CONTROL_PROTOCOL_VERSION": 1,
        "COMMAND_CONFIRM_COUNT": 3,
        "COMMAND_CONFIRM_WINDOW_SEC": 2.5,
        "MIN_LIMITED_SPEED_MPS": 0.5,
        "MAX_LIMITED_SPEED_MPS": 15.0,
    }
    main = next(node for node in tree.body
                if isinstance(node, ast.FunctionDef) and node.name == "main")
    arm_waiter = next(node for node in main.body
                      if isinstance(node, ast.FunctionDef) and node.name == "_wait_for_arm")
    module = ast.Module(body=[*helpers, gate_class, arm_waiter], type_ignores=[])
    module = ast.fix_missing_locations(module)
    exec(compile(module, str(SCRIPT_PATH), "exec"), namespace)
    return namespace


PROTOCOL = _load_protocol_code()
parse_control_packet = PROTOCOL["parse_control_packet"]
ProtocolGate = PROTOCOL["ProtocolGate"]


def _message(
    repeat_index=1,
    sequence=100,
    command_id="cmd-100",
    session_id="backend-test",
):
    return {
        "protocol": "ue5_drone_control",
        "version": 1,
        "type": "control",
        "session_id": session_id,
        "command_id": command_id,
        "sequence": sequence,
        "drone_id": 1,
        "slot": 1,
        "mode": "move",
        "issued_at_unix_s": 1000.0,
        "sent_at_unix_s": 1000.1,
        "target": {
            "frame": "NED",
            "reference": "power_on_origin",
            "unit": "m",
            "north": 10.0,
            "east": 20.0,
            "down": -5.0,
        },
        "delivery": {"repeat_index": repeat_index, "repeat_total": 5},
    }


def _new_gate():
    gate = ProtocolGate()
    gate._active_backend_session = None
    gate._retired_backend_sessions = set()
    gate._highest_applied_sequence = 0
    gate._pending_commands = {}
    gate._applied_command_ids = {}
    gate._commands_applied = 0
    gate._last_applied_command = None
    gate._udp_rx_duplicate = 0
    gate._udp_rx_stale = 0
    gate._udp_rx_invalid = 0
    gate._last_setpoint = {
        "x": 0.0, "y": 0.0, "z": 0.0,
        "mode": "hold", "sequence": 0,
    }
    gate._setpoint_lock = threading.Lock()
    gate.get_logger = lambda: _Logger()
    return gate


def _invalid_numeric_packets():
    fields = [
        (None, "sequence"), (None, "drone_id"), (None, "slot"),
        (None, "issued_at_unix_s"), (None, "sent_at_unix_s"),
        ("target", "north"), ("target", "east"), ("target", "down"),
        ("delivery", "repeat_index"), ("delivery", "repeat_total"),
        (None, "speed"),
    ]
    for parent, field in fields:
        for value in (float("inf"), float("-inf"), float("nan"), 10 ** 400):
            message = _message()
            target = message if parent is None else message[parent]
            target[field] = value
            # 1e309 is a valid JSON number which overflows Python's float. Also
            # exercise NaN as received from permissive JSON senders.
            packet = json.dumps(message).encode("utf-8").replace(b"Infinity", b"1e309")
            yield (parent, field, str(value)), packet


class ControlProtocolTest(unittest.TestCase):
    def test_parser_accepts_explicit_power_on_relative_ned_meters(self):
        parsed = parse_control_packet(json.dumps(_message()).encode("utf-8"))
        self.assertEqual(parsed["x"], 10.0)
        self.assertEqual(parsed["y"], 20.0)
        self.assertEqual(parsed["z"], -5.0)

    def test_parser_rejects_wrong_coordinate_unit(self):
        message = _message()
        message["target"]["unit"] = "cm"
        with self.assertRaises(ValueError):
            parse_control_packet(json.dumps(message).encode("utf-8"))

    def test_parser_rejects_nonfinite_and_oversized_numeric_fields(self):
        for case, packet in _invalid_numeric_packets():
            with self.subTest(case=case):
                self.assertLess(len(packet), PROTOCOL["MAX_CONTROL_PACKET_BYTES"])
                with self.assertRaises(ValueError):
                    parse_control_packet(packet)

    def test_parser_preserves_backend_integer_boundaries_and_speed_markers(self):
        message = _message(sequence=(1 << 64) - 1)
        message.update(drone_id=(1 << 31) - 1, slot=(1 << 31) - 1)
        message["delivery"] = {"repeat_index": (1 << 32) - 1, "repeat_total": 0}
        for speed in (None, 0, "NaN", 0.5, 15.0):
            with self.subTest(speed=speed):
                if speed is None:
                    message.pop("speed", None)
                else:
                    message["speed"] = speed
                parsed = parse_control_packet(json.dumps(message).encode("utf-8"))
                self.assertEqual(parsed["sequence"], (1 << 64) - 1)
                self.assertEqual(parsed["repeat_index"], (1 << 32) - 1)
                if speed in (None, 0, "NaN"):
                    self.assertTrue(math.isnan(parsed["speed_mps"]))
                else:
                    self.assertEqual(parsed["speed_mps"], speed)

    def test_receive_loop_rejects_bad_numbers_then_applies_next_valid_command(self):
        sender = ("127.0.0.1", 50123)
        for case, bad_packet in _invalid_numeric_packets():
            with self.subTest(case=case):
                gate = _new_gate()
                gate.slot = 1
                gate._tel_port = 8888
                gate._backend_addr = (sender[0], gate._tel_port)
                gate._last_ctrl_sender = None
                gate._last_clock_warning_monotonic = 0
                gate._udp_rx_total = gate._udp_rx_valid = 0
                gate._udp_rx_hold = gate._udp_rx_move = 0
                gate._maybe_reply_ping = lambda _data, _addr: False
                packets = [bad_packet]
                for index in (1, 2, 3):
                    message = _message(repeat_index=index)
                    message["sent_at_unix_s"] = time.time()
                    packets.append(json.dumps(message).encode("utf-8"))
                gate._ctrl_sock = Mock()
                gate._ctrl_sock.recvfrom.side_effect = [(packet, sender) for packet in packets]
                for _ in packets:
                    gate.process_control()
                self.assertEqual(gate._udp_rx_invalid, 1)
                self.assertEqual(gate._udp_rx_total, 4)
                self.assertEqual(gate._udp_rx_valid, 3)
                self.assertEqual(gate._commands_applied, 1)
                self.assertEqual(gate._last_setpoint["sequence"], 100)

    def test_three_unique_repeats_apply_once(self):
        gate = _new_gate()
        sender = ("192.168.30.100", 50123)
        for index in (1, 2):
            packet = parse_control_packet(
                json.dumps(_message(repeat_index=index)).encode("utf-8")
            )
            self.assertTrue(gate._accept_backend_session(packet))
            gate._stage_control_command(packet, sender, 10.0 + index * 0.1)
        self.assertEqual(gate._commands_applied, 0)

        packet = parse_control_packet(
            json.dumps(_message(repeat_index=3)).encode("utf-8")
        )
        gate._stage_control_command(packet, sender, 10.3)
        self.assertEqual(gate._commands_applied, 1)
        self.assertEqual(gate._last_setpoint["sequence"], 100)
        self.assertEqual(gate._last_setpoint["z"], -5.0)

    def test_duplicate_repeat_index_does_not_reach_threshold(self):
        gate = _new_gate()
        sender = ("192.168.30.100", 50123)
        for index in (1, 1, 2):
            packet = parse_control_packet(
                json.dumps(_message(repeat_index=index)).encode("utf-8")
            )
            gate._accept_backend_session(packet)
            gate._stage_control_command(packet, sender, 20.0 + index * 0.1)
        self.assertEqual(gate._commands_applied, 0)
        self.assertEqual(gate._udp_rx_duplicate, 1)

    def test_older_sequence_is_rejected_after_newer_command(self):
        gate = _new_gate()
        sender = ("192.168.30.100", 50123)
        for index in (1, 2, 3):
            packet = parse_control_packet(json.dumps(_message(
                repeat_index=index, sequence=101, command_id="cmd-101"
            )).encode("utf-8"))
            gate._accept_backend_session(packet)
            gate._stage_control_command(packet, sender, 30.0 + index * 0.1)
        old = parse_control_packet(json.dumps(_message(
            repeat_index=4, sequence=100, command_id="cmd-100"
        )).encode("utf-8"))
        gate._stage_control_command(old, sender, 31.0)
        self.assertEqual(gate._commands_applied, 1)
        self.assertEqual(gate._udp_rx_stale, 1)

    def test_same_command_id_with_conflicting_target_is_discarded(self):
        gate = _new_gate()
        sender = ("192.168.30.100", 50123)
        first = _message(repeat_index=1)
        conflict = _message(repeat_index=2)
        conflict["target"]["north"] = 999.0

        for message in (first, conflict):
            packet = parse_control_packet(json.dumps(message).encode("utf-8"))
            gate._accept_backend_session(packet)
            gate._stage_control_command(packet, sender, 40.0)

        self.assertEqual(gate._commands_applied, 0)
        self.assertEqual(gate._udp_rx_invalid, 1)
        self.assertNotIn("cmd-100", gate._pending_commands)

    def test_new_session_retires_old_session_and_restarts_ordering(self):
        gate = _new_gate()
        sender = ("192.168.30.100", 50123)

        old = parse_control_packet(json.dumps(_message(
            repeat_index=1,
            sequence=500,
            command_id="old-500",
            session_id="backend-old",
        )).encode("utf-8"))
        self.assertTrue(gate._accept_backend_session(old))
        gate._stage_control_command(old, sender, 50.0)

        for index in (1, 2, 3):
            new = parse_control_packet(json.dumps(_message(
                repeat_index=index,
                sequence=1,
                command_id="new-1",
                session_id="backend-new",
            )).encode("utf-8"))
            self.assertTrue(gate._accept_backend_session(new))
            gate._stage_control_command(new, sender, 50.0 + index * 0.1)

        self.assertEqual(gate._commands_applied, 1)
        self.assertEqual(gate._highest_applied_sequence, 1)
        self.assertFalse(gate._accept_backend_session(old))
        self.assertEqual(gate._udp_rx_stale, 1)


class ArmConfirmationTest(unittest.TestCase):
    def _wait_for_arm(self, input_stream, *, arm_now=None, running=True):
        bridge = types.SimpleNamespace(
            _warmup_count=1, _warmup_needed=1, _arm_triggered=False,
            get_logger=lambda: _Logger(),
        )
        fake_sys = types.SimpleNamespace(stdin=input_stream, stdout=io.StringIO())
        with patch.dict(PROTOCOL, {"bridge": bridge, "sys": fake_sys, "running": running}), \
                patch.dict(os.environ), contextlib.redirect_stdout(io.StringIO()):
            os.environ.pop("ARM_NOW", None)
            if arm_now is not None:
                os.environ["ARM_NOW"] = arm_now
            PROTOCOL["_wait_for_arm"]()
        return bridge._arm_triggered

    def test_eof_unavailable_and_incomplete_input_never_arm(self):
        for source in (io.StringIO(""), io.StringIO("yes"), None):
            with self.subTest(source=source):
                self.assertFalse(self._wait_for_arm(source))
        for error in (EOFError(), OSError("stdin unavailable"), ValueError("closed input")):
            with self.subTest(error=type(error).__name__):
                source = Mock()
                source.readline.side_effect = error
                self.assertFalse(self._wait_for_arm(source))

    def test_enter_confirms_arm_and_only_explicit_arm_now_bypasses_input(self):
        self.assertTrue(self._wait_for_arm(io.StringIO("\n")))
        self.assertTrue(self._wait_for_arm(None, arm_now="1"))
        for setting in ("0", "true", ""):
            with self.subTest(arm_now=setting):
                self.assertFalse(self._wait_for_arm(io.StringIO(""), arm_now=setting))

    def test_stop_before_or_during_input_never_arms(self):
        source = Mock()
        source.readline.side_effect = AssertionError("stopped bridge must not read input")
        self.assertFalse(self._wait_for_arm(source, running=False))
        self.assertFalse(self._wait_for_arm(source, running=False, arm_now="1"))

        def stop_during_read():
            PROTOCOL["running"] = False
            return "\n"

        source.readline.side_effect = stop_during_read
        self.assertFalse(self._wait_for_arm(source))


if __name__ == "__main__":
    unittest.main()
