#include "udp_receiver.h"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace {

bool TryParseFiniteDouble(const YAML::Node& node, double& out_value)
{
    if (!node || !node.IsScalar()) {
        return false;
    }

    try {
        const double value = node.as<double>();
        if (!std::isfinite(value)) {
            return false;
        }
        out_value = value;
        return true;
    } catch (const YAML::Exception&) {
        return false;
    }
}

bool TryParseBool(const YAML::Node& node, bool& out_value)
{
    if (!node || !node.IsScalar()) {
        return false;
    }

    try {
        out_value = node.as<bool>();
        return true;
    } catch (const YAML::Exception&) {
        return false;
    }
}

template <std::size_t N>
bool TryParseFiniteVector(const YAML::Node& node, double (&out_value)[N])
{
    if (!node || !node.IsSequence() || node.size() < N) {
        return false;
    }

    double values[N];
    for (std::size_t i = 0; i < N; ++i) {
        if (!TryParseFiniteDouble(node[i], values[i])) {
            return false;
        }
    }
    std::copy_n(values, N, out_value);
    return true;
}

bool TryParseStatusByte(const YAML::Node& node, uint8_t& out_value)
{
    if (!node || !node.IsScalar()) {
        return false;
    }
    const int value = node.as<int>(-1);
    if (value < 0 || value > UINT8_MAX) {
        return false;
    }
    out_value = static_cast<uint8_t>(value);
    return true;
}

}  // namespace

UdpReceiver::UdpReceiver(boost::asio::io_context& io_context)
    : io_context_(io_context)
{
}

UdpReceiver::~UdpReceiver()
{
    Stop();
}

void UdpReceiver::AddPort(int slot, int port, int drone_id)
{
    auto listener = std::make_unique<PortListener>(
        PortListener{slot, port, drone_id,
                     boost::asio::ip::udp::socket(io_context_),
                     boost::asio::ip::udp::endpoint(),
                     std::array<char, 65535>{}});

    listeners_.push_back(std::move(listener));
    spdlog::info("[UdpReceiver] Slot {}: listening port {} for drone {}",
                 slot, port, drone_id);
}

void UdpReceiver::SetCallback(ReceiveCallback cb)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callback_ = std::move(cb);
}

void UdpReceiver::SetPongCallback(PongCallback cb)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);
    pong_callback_ = std::move(cb);
}

void UdpReceiver::SetAckCallback(AckCallback cb)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);
    ack_callback_ = std::move(cb);
}

void UdpReceiver::Start()
{
    if (running_) return;
    running_ = true;

    for (auto& listener : listeners_) {
        try {
            listener->socket.open(boost::asio::ip::udp::v4());
            listener->socket.set_option(boost::asio::socket_base::reuse_address(true));
            listener->socket.bind(boost::asio::ip::udp::endpoint(
                boost::asio::ip::address_v4::any(), listener->port));
            spdlog::info("[UdpReceiver] Listening on port {}", listener->port);
            StartReceive(*listener);
        } catch (const std::exception& e) {
            spdlog::error("[UdpReceiver] Failed to bind port {}: {}", listener->port, e.what());
        }
    }
}

void UdpReceiver::Stop()
{
    if (!running_) return;
    running_ = false;

    for (auto& listener : listeners_) {
        try {
            listener->socket.close();
        } catch (...) {}
    }
    spdlog::info("[UdpReceiver] Stopped");
}

void UdpReceiver::StartReceive(PortListener& listener)
{
    if (!running_) return;

    listener.socket.async_receive_from(
        boost::asio::buffer(listener.buffer), listener.remote_endpoint,
        [this, &listener](const boost::system::error_code& error, size_t bytes) {
            HandleReceive(listener, error, bytes);
        });
}

void UdpReceiver::HandleReceive(PortListener& listener,
                                 const boost::system::error_code& error,
                                 size_t bytes_transferred)
{
    if (!running_) return;

    if (!error && bytes_transferred > 0) {
        try {
            // [修复 #3]: yaml-cpp 需要 null-terminated 字符串，不能用 string_view
            std::string yaml_str(listener.buffer.data(), bytes_transferred);
            YAML::Node root = YAML::Load(yaml_str);

            // 延迟探测 pong：JSON 是合法 YAML，type=="pong" 时直接分流，
            // 不进入下面的遥测字段解析（字段语义完全不同）。
            if (auto type_node = root["type"]; type_node && type_node.IsScalar()
                    && type_node.as<std::string>("") == "pong") {
                uint64_t ping_id = 0;
                double echoed_sent_at = 0.0;
                if (auto v = root["ping_id"]) ping_id = v.as<uint64_t>(0);
                if (auto v = root["sent_at_unix_s"]) echoed_sent_at = v.as<double>(0.0);

                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (pong_callback_) {
                    pong_callback_(listener.slot, ping_id, echoed_sent_at);
                }
                StartReceive(listener);
                return;
            }

            // Control ACK: independent from telemetry.
            // Never pass an ACK-only packet into DroneManager::HandleTelemetry().
            if (auto type_node = root["type"];
                type_node && type_node.IsScalar()
                && type_node.as<std::string>("") == "control_ack") {

                ControlAckData ack{};

                ack.slot = listener.slot;

                if (auto v = root["slot"]) {
                    const int packet_slot = v.as<int>(0);
                    if (packet_slot != listener.slot) {
                        spdlog::warn(
                            "[UdpReceiver] ACK slot mismatch: listener={} packet={}",
                            listener.slot, packet_slot);
                        StartReceive(listener);
                        return;
                    }
                }

                if (auto v = root["drone_id"]) {
                    ack.drone_id = v.as<int>(0);
                    if (ack.drone_id <= 0) {
                        spdlog::warn("[UdpReceiver] Invalid ACK drone ID for slot {}", listener.slot);
                        StartReceive(listener);
                        return;
                    }
                }

                if (auto v = root["status"]) {
                    ack.status = v.as<std::string>("");
                }

                if (auto v = root["message"]) {
                    ack.message = v.as<std::string>("");
                }

                if (auto v = root["session_id"]) {
                    ack.session_id = v.as<std::string>("");
                }

                if (auto v = root["command_id"]) {
                    ack.command_id = v.as<std::string>("");
                }

                if (auto v = root["sequence"]) {
                    ack.sequence = v.as<uint64_t>(0);
                }

                if (auto v = root["mode"]) {
                    ack.mode = v.as<std::string>("");
                }

                if (auto v = root["confirmed_packets"]) {
                    ack.confirmed_packets = v.as<uint32_t>(0);
                }

                if (auto v = root["timestamp"]) {
                    ack.timestamp = v.as<uint64_t>(0);
                }

                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (ack_callback_) {
                        ack_callback_(listener.slot, ack);
                    }
                }

                spdlog::debug(
                    "[UdpReceiver] Slot {} received control ACK: status={} sequence={}",
                    listener.slot, ack.status, ack.sequence);

                StartReceive(listener);
                return;
            }

            TelemetryData tel{};

            // timestamp (微秒)
            if (auto ts = root["timestamp"]) {
                tel.timestamp = ts.as<uint64_t>(0);
            }

            // Legacy position [N, E, D].
            // Keep the existing legacy format; do not substitute VehicleOdometry here.
            if (auto pos = root["position"]) {
                if (pos.IsSequence() && pos.size() >= 3) {
                    tel.position_ned[0] = pos[0].as<double>(0.0);
                    tel.position_ned[1] = pos[1].as<double>(0.0);
                    tel.position_ned[2] = pos[2].as<double>(0.0);
                }
            }

            // Quaternion [w, x, y, z].
            const YAML::Node attitude = root["attitude"];
            if (!attitude || !attitude.IsMap() ||
                !TryParseFiniteVector(attitude["q"], tel.quaternion)) {
                TryParseFiniteVector(root["q"], tel.quaternion);
            }

            // Velocity [vN, vE, vD].
            const YAML::Node odometry = root["odometry"];
            if (!odometry || !odometry.IsMap() ||
                !TryParseFiniteVector(odometry["velocity"], tel.velocity)) {
                TryParseFiniteVector(root["velocity"], tel.velocity);
            }

            // angular_velocity
            if (auto av = root["angular_velocity"]) {
                if (av.IsSequence() && av.size() >= 3) {
                    tel.angular_velocity[0] = av[0].as<double>(0.0);
                    tel.angular_velocity[1] = av[1].as<double>(0.0);
                    tel.angular_velocity[2] = av[2].as<double>(0.0);
                }
            }

            // battery
            if (auto bat = root["battery"]) {
                tel.battery = bat.as<int>(-1);
            }

            // WGS84 global position. Geographic dispatch requires a complete,
            // finite latitude/longitude/AMSL tuple.
            const auto gps_lat = root["gps_lat"];
            const auto gps_lon = root["gps_lon"];
            const auto gps_alt = root["gps_alt"];
            const auto gps_fix = root["gps_fix"];
            double parsed_gps_lat = 0.0;
            double parsed_gps_lon = 0.0;
            double parsed_gps_alt = 0.0;
            bool parsed_gps_fix = false;
            const bool has_complete_gps =
                TryParseFiniteDouble(gps_lat, parsed_gps_lat)
                && TryParseFiniteDouble(gps_lon, parsed_gps_lon)
                && TryParseFiniteDouble(gps_alt, parsed_gps_alt);
            const bool has_gps_fix_flag = TryParseBool(gps_fix, parsed_gps_fix);
            if (has_complete_gps) {
                tel.gps_lat = parsed_gps_lat;
                tel.gps_lon = parsed_gps_lon;
                tel.gps_alt = parsed_gps_alt;
            }
            tel.gps_fix = has_gps_fix_flag && parsed_gps_fix
                && has_complete_gps
                && tel.gps_lat >= -90.0 && tel.gps_lat <= 90.0
                && tel.gps_lon >= -180.0 && tel.gps_lon <= 180.0;

            // VehicleLocalPosition [N, E, D] is the coordinate frame consumed
            // by PX4 TrajectorySetpoint. Do not substitute VehicleOdometry.
            bool has_local_position = false;
            if (auto lp = root["local_position"]) {
                if (lp.IsSequence() && lp.size() >= 3) {
                    double parsed_local_position[3]{};
                    has_local_position =
                        TryParseFiniteDouble(lp[0], parsed_local_position[0])
                        && TryParseFiniteDouble(lp[1], parsed_local_position[1])
                        && TryParseFiniteDouble(lp[2], parsed_local_position[2]);
                    if (has_local_position) {
                        tel.local_position[0] = parsed_local_position[0];
                        tel.local_position[1] = parsed_local_position[1];
                        tel.local_position[2] = parsed_local_position[2];
                    }
                }
            }
            bool parsed_local_position_valid = false;
            const bool has_local_position_valid_flag =
                TryParseBool(root["local_position_valid"], parsed_local_position_valid);
            tel.local_position_valid = has_local_position_valid_flag
                && parsed_local_position_valid
                && has_local_position;
            if (tel.local_position_valid) {
                tel.position_ned[0] = tel.local_position[0];
                tel.position_ned[1] = tel.local_position[1];
                tel.position_ned[2] = tel.local_position[2];
            }

            // arming_state / nav_state.
            // Prefer nested vehicle_status; retain legacy top-level compatibility.
            const YAML::Node vehicle_status = root["vehicle_status"];
            const bool has_vehicle_status = vehicle_status && vehicle_status.IsMap();
            if (!has_vehicle_status ||
                !TryParseStatusByte(vehicle_status["arming_state"], tel.arming_state)) {
                TryParseStatusByte(root["arming_state"], tel.arming_state);
            }
            if (!has_vehicle_status ||
                !TryParseStatusByte(vehicle_status["nav_state"], tel.nav_state)) {
                TryParseStatusByte(root["nav_state"], tel.nav_state);
            }

            // Jetson 达到 3-of-5 确认阈值并真正应用 setpoint 后返回的应用层 ACK。
            if (auto ack = root["control_ack"]; ack && ack.IsMap()) {
                if (auto value = ack["session_id"]) {
                    tel.control_ack_session_id = value.as<std::string>("");
                }
                if (auto value = ack["command_id"]) {
                    tel.control_ack_command_id = value.as<std::string>("");
                }
                if (auto value = ack["sequence"]) {
                    tel.control_ack_sequence = value.as<uint64_t>(0);
                }
                if (auto value = ack["mode"]) {
                    tel.control_ack_mode = value.as<std::string>("");
                }
                if (auto value = ack["confirmed_packets"]) {
                    tel.control_ack_confirmed_packets = value.as<uint32_t>(0);
                }
            }

            spdlog::debug("[UdpReceiver] Slot {} recv {}B from {}:{}: NED({:.2f},{:.2f},{:.2f}) bat={} gps_fix={}",
                         listener.slot, bytes_transferred,
                         listener.remote_endpoint.address().to_string(),
                         listener.remote_endpoint.port(),
                         tel.position_ned[0], tel.position_ned[1], tel.position_ned[2],
                         tel.battery, tel.gps_fix);

            // 回调通知（线程安全）
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (callback_) {
                    callback_(listener.slot, tel);
                }
            }

        } catch (const YAML::Exception& e) {
            spdlog::warn("[UdpReceiver] YAML parse error on port {}: {}",
                         listener.port, e.what());
        } catch (const std::exception& e) {
            spdlog::warn("[UdpReceiver] Parse error on port {}: {}",
                         listener.port, e.what());
        }
    }

    // 继续接收下一个包
    StartReceive(listener);
}
