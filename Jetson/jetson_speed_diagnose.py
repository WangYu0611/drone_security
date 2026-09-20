#!/usr/bin/env python3
"""Read-only diagnosis for the Jetson speed-control integration.

This script never sends UDP packets, publishes ROS messages, arms a vehicle,
or changes any file.  It only prints information needed to add the `speed`
field safely to jetson_bridge.py and PX4 TrajectorySetpoint handling.

Run from the Jetson after sourcing ROS 2:
    source /opt/ros/humble/setup.bash
    python3 jetson_speed_diagnose.py

If the bridge file lives elsewhere:
    python3 jetson_speed_diagnose.py --bridge /absolute/path/jetson_bridge.py
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
from pathlib import Path
import platform
import re
import shutil
import socket
import subprocess
import sys
from typing import Iterable


OUTPUT: list[str] = []


def section(title: str) -> None:
    OUTPUT.append("\n" + "=" * 72)
    OUTPUT.append(title)
    OUTPUT.append("=" * 72)


def line(text: str = "") -> None:
    OUTPUT.append(text)


def run(command: list[str], timeout: int = 8) -> None:
    line("$ " + " ".join(command))
    if not shutil.which(command[0]):
        line(f"[NOT FOUND] {command[0]}")
        return
    try:
        completed = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired:
        line(f"[TIMEOUT after {timeout}s]")
        return
    output = completed.stdout.strip()
    line(output if output else "[no output]")
    if completed.returncode:
        line(f"[exit code {completed.returncode}]")


def source_context(lines: list[str], keywords: Iterable[str], radius: int = 3) -> None:
    found: set[int] = set()
    lowered = tuple(keyword.lower() for keyword in keywords)
    for index, value in enumerate(lines):
        if any(keyword in value.lower() for keyword in lowered):
            for context_index in range(max(0, index - radius), min(len(lines), index + radius + 1)):
                found.add(context_index)
    if not found:
        line("[No matching source lines]")
        return
    previous = -2
    for index in sorted(found):
        if index > previous + 1:
            line("...")
        line(f"{index + 1:4d}: {lines[index]}")
        previous = index


def find_bridge(explicit_path: str | None) -> Path | None:
    candidates: list[Path] = []
    if explicit_path:
        candidates.append(Path(explicit_path).expanduser())
    here = Path(__file__).resolve().parent
    candidates.extend((here / "jetson_bridge.py", Path.cwd() / "jetson_bridge.py"))
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Read-only Jetson speed-control diagnosis")
    parser.add_argument("--bridge", help="Absolute path to the active jetson_bridge.py")
    args = parser.parse_args()

    section("JETSON SPEED CONTROL DIAGNOSIS (READ-ONLY)")
    line(f"time: {dt.datetime.now().astimezone().isoformat(timespec='seconds')}")
    line(f"host: {socket.gethostname()}")
    line(f"platform: {platform.platform()}")
    line(f"python: {sys.version.split()[0]} ({sys.executable})")
    line(f"ROS_DISTRO: {os.environ.get('ROS_DISTRO', '<not sourced>')}")
    line(f"ROS_TOPIC_PREFIX: {os.environ.get('ROS_TOPIC_PREFIX', '<empty>')}")
    line(f"CONTROL_PORT: {os.environ.get('CONTROL_PORT', '<bridge default>')}")
    line(f"BACKEND_HOST: {os.environ.get('BACKEND_HOST', '<bridge default>')}")

    section("ACTIVE BRIDGE SOURCE")
    bridge = find_bridge(args.bridge)
    if bridge is None:
        line("[NOT FOUND] Supply --bridge /absolute/path/jetson_bridge.py")
    else:
        line(f"bridge: {bridge}")
        source_lines = bridge.read_text(encoding="utf-8", errors="replace").splitlines()
        line("\n-- protocol / UDP / speed-related source --")
        source_context(
            source_lines,
            ("CONTROL_PORT", "parse_control_packet", "speed", "TrajectorySetpoint", "OffboardControlMode"),
            radius=3,
        )

        # This direct check makes the most important implementation gap unambiguous.
        parse_start = next((i for i, text in enumerate(source_lines) if text.startswith("def parse_control_packet")), None)
        parse_end = next(
            (i for i, text in enumerate(source_lines[parse_start + 1 if parse_start is not None else 0:], start=(parse_start or 0) + 1)
             if text.startswith("class ")),
            len(source_lines),
        )
        parse_block = "\n".join(source_lines[parse_start:parse_end]) if parse_start is not None else ""
        line("\n-- speed field accepted by parse_control_packet --")
        line("YES" if re.search(r"message\.get\([\"']speed[\"']\)|message\[[\"']speed[\"']\]", parse_block) else "NO")

    section("PX4 / ROS 2 MESSAGE DEFINITIONS")
    run(["ros2", "pkg", "prefix", "px4_msgs"])
    run(["ros2", "interface", "show", "px4_msgs/msg/TrajectorySetpoint"])
    run(["ros2", "interface", "show", "px4_msgs/msg/OffboardControlMode"])

    section("LIVE ROS 2 GRAPH")
    run(["ros2", "node", "list"])
    run(["ros2", "topic", "list", "-t"])
    for topic in (
        "/fmu/in/trajectory_setpoint",
        "/fmu/in/offboard_control_mode",
        "/fmu/out/vehicle_local_position",
        "/fmu/out/vehicle_odometry",
    ):
        run(["ros2", "topic", "info", "-v", topic])

    section("PROCESS / UDP LISTENERS")
    run(["ps", "-ef"])
    if shutil.which("ss"):
        run(["ss", "-lunp"])
    else:
        run(["netstat", "-lunp"])

    section("PYTHON PACKAGE IMPORTS")
    run([sys.executable, "-c", "import rclpy; print('rclpy import: OK')"])
    run([
        sys.executable,
        "-c",
        "from px4_msgs.msg import TrajectorySetpoint, OffboardControlMode; "
        "print('px4_msgs import: OK'); print(TrajectorySetpoint()); print(OffboardControlMode())",
    ])

    section("WHAT TO SEND BACK")
    line("Send the complete generated .txt file. Do not start/stop the bridge for this test.")
    line("We need: the speed-field acceptance result, TrajectorySetpoint definition,")
    line("bridge source excerpt, ROS graph, and the UDP listener line for CONTROL_PORT.")

    report = "\n".join(OUTPUT) + "\n"
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    report_path = Path.cwd() / f"jetson_speed_diagnose_{stamp}.txt"
    report_path.write_text(report, encoding="utf-8")
    print(report)
    print(f"\nSaved report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
