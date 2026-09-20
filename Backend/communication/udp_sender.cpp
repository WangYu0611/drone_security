#include "udp_sender.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>

namespace {

double now_unix_seconds()
{
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count())
        / 1000000.0;
}

std::string make_session_id()
{
    const auto now_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    std::random_device random;
    std::ostringstream stream;
    stream << "backend-" << now_us << '-' << std::hex << std::setw(8)
           << std::setfill('0') << random();
    return stream.str();
}

} // namespace

UdpSender::UdpSender(boost::asio::io_context& io_context)
    : io_context_(io_context)
    , session_id_(make_session_id())
{
    spdlog::info("[UdpSender] JSON protocol session_id={}", session_id_);
}

UdpSender::~UdpSender()
{
    spdlog::info("[UdpSender] Destroyed");
}

void UdpSender::SetTarget(int drone_id, const std::string& host, int port)
{
    auto target = std::make_unique<Target>(io_context_);
    target->host = host;
    target->port = port;
    try {
        target->endpoint = boost::asio::ip::udp::endpoint(
            boost::asio::ip::make_address(host), static_cast<unsigned short>(port));
        target->initialized = true;
    } catch (const std::exception& e) {
        spdlog::error("[UdpSender] Invalid target for drone {}: {}:{} ({})",
                      drone_id, host, port, e.what());
        target->initialized = false;
    }

    std::lock_guard<std::mutex> lock(targets_mutex_);
    targets_[drone_id] = std::move(target);
    spdlog::info("[UdpSender] Drone {} target set: {}:{}", drone_id, host, port);
}

bool UdpSender::Send(int drone_id, const DroneControlPacket& packet)
{
    const std::string mode = packet.mode == 1 ? "move" : "hold";
    const std::string command_id = session_id_ + "-d" +
        std::to_string(drone_id) + "-s" + std::to_string(packet.sequence);

    nlohmann::json json = {
        {"protocol", "ue5_drone_control"},
        {"version", 1},
        {"type", "control"},
        {"session_id", session_id_},
        {"command_id", command_id},
        {"sequence", packet.sequence},
        {"drone_id", drone_id},
        {"slot", packet.slot},
        {"mode", mode},
        {"issued_at_unix_s", packet.timestamp},
        {"sent_at_unix_s", now_unix_seconds()},
        {"target", {
            {"frame", "NED"},
            {"reference", "power_on_origin"},
            {"unit", "m"},
            {"north", packet.x},
            {"east", packet.y},
            {"down", packet.z},
        }},
        {"delivery", {
            {"repeat_index", packet.repeat_index},
            {"repeat_total", packet.repeat_total},
        }},
    };
    if (packet.mode == 1 && std::isfinite(packet.speed_mps) && packet.speed_mps >= 0.5f) {
        // Finite value makes Jetson generate a velocity setpoint at this 3D speed.
        json["speed"] = packet.speed_mps;
    } else if (packet.use_unlimited_speed) {
        // JSON has no portable NaN number literal. Jetson's control adapter
        // accepts this explicit float marker and converts it via float("nan").
        json["speed"] = "NaN";
    }
    const std::string payload = json.dump();

    try {
        // SetTarget 可能替换 Target；同步发送期间保持锁以避免悬空指针。
        std::lock_guard<std::mutex> lock(targets_mutex_);
        auto it = targets_.find(drone_id);
        if (it == targets_.end() || !it->second || !it->second->initialized) {
            spdlog::error("[UdpSender] No initialized target for drone {}", drone_id);
            return false;
        }
        auto& target = *it->second;
        const auto bytes = target.socket.send_to(
            boost::asio::buffer(payload), target.endpoint);
        if (bytes != payload.size()) {
            spdlog::error(
                "[UdpSender] Short UDP send drone {}: {}/{} bytes",
                drone_id, bytes, payload.size());
            return false;
        }
        spdlog::debug(
            "[UdpSender] JSON drone={} slot={} id={} mode={} repeat={}/{} "
            "NED=({:.3f},{:.3f},{:.3f}) bytes={} -> {}:{}",
            drone_id, packet.slot, command_id, mode,
            packet.repeat_index, packet.repeat_total,
            packet.x, packet.y, packet.z, bytes, target.host, target.port);
        return true;
    } catch (const std::exception& e) {
        spdlog::error(
            "[UdpSender] JSON send to drone {} failed: {}", drone_id, e.what());
        return false;
    }
}

bool UdpSender::SendPing(int drone_id, uint64_t ping_id)
{
    nlohmann::json json = {
        {"protocol", "ue5_drone_control"},
        {"version", 1},
        {"type", "ping"},
        {"session_id", session_id_},
        {"ping_id", ping_id},
        {"sent_at_unix_s", now_unix_seconds()},
    };
    const std::string payload = json.dump();

    try {
        std::lock_guard<std::mutex> lock(targets_mutex_);
        auto it = targets_.find(drone_id);
        if (it == targets_.end() || !it->second || !it->second->initialized) {
            spdlog::error("[UdpSender] No initialized target for drone {} (ping)", drone_id);
            return false;
        }
        auto& target = *it->second;
        const auto bytes = target.socket.send_to(
            boost::asio::buffer(payload), target.endpoint);
        if (bytes != payload.size()) {
            spdlog::error(
                "[UdpSender] Short UDP ping send drone {}: {}/{} bytes",
                drone_id, bytes, payload.size());
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error(
            "[UdpSender] Ping send to drone {} failed: {}", drone_id, e.what());
        return false;
    }
}
