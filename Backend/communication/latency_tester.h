#pragma once

#include "udp_sender.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

/// UDP 控制链路延迟测试（后端 <-> Jetson）。
///
/// 只用后端自身的 steady_clock 计算往返时延（RTT），不依赖 Jetson 的系统时钟，
/// 因此两端时钟不同步也不影响结果。走 ping/pong 协议，独立于 move/hold 控制
/// 状态机，不会移动无人机、不占用 sequence，也不受 3-of-5 确认阈值影响。
///
/// 用法（CLI 线程调用，阻塞直到测完 count 次或超时）：
///   LatencyTester tester(sender);
///   udp_receiver.SetPongCallback([&](int slot, uint64_t id, double t) {
///       tester.OnPong(slot, id, t);
///   });
///   auto result = tester.RunTest(drone_id, slot, count, timeout_ms);
class LatencyTester {
public:
    struct Result {
        int sent = 0;
        int received = 0;
        std::vector<double> rtt_ms;  // 每次成功往返的 RTT（毫秒），按发送顺序
        double min_ms = 0.0;
        double avg_ms = 0.0;
        double max_ms = 0.0;
        bool any_success = false;
    };

    explicit LatencyTester(UdpSender& sender) : sender_(sender) {}

    /// 收到 pong 时由 UdpReceiver 的回调线程调用。
    void OnPong(int slot, uint64_t ping_id, double /*echoed_sent_at_unix_s*/)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_.find(ping_id);
        if (it == pending_.end() || it->second.slot != slot) {
            return;  // 迟到的旧探测包或槽位不匹配，丢弃
        }
        it->second.rtt_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - it->second.sent_at).count();
        it->second.done = true;
        cv_.notify_all();
    }

    /// 阻塞执行一次延迟测试。CLI 线程直接调用。
    /// @param drone_id    目标无人机 ID（UdpSender::SendPing 用它找发送目标）
    /// @param slot        目标槽位（校验 pong 来源槽位，与 drone_id 对应）
    /// @param count       探测次数
    /// @param timeout_ms  单次探测的超时（毫秒）
    Result RunTest(int drone_id, int slot, int count, int timeout_ms)
    {
        Result result;
        for (int i = 0; i < count; ++i) {
            const uint64_t ping_id = NextPingId();

            {
                std::lock_guard<std::mutex> lock(mutex_);
                pending_[ping_id] = PendingPing{slot, std::chrono::steady_clock::now(), false, 0.0};
            }

            result.sent += 1;
            const bool sent_ok = sender_.SendPing(drone_id, ping_id);

            if (sent_ok) {
                std::unique_lock<std::mutex> lock(mutex_);
                auto it = pending_.find(ping_id);
                const bool got_pong = cv_.wait_for(
                    lock, std::chrono::milliseconds(timeout_ms),
                    [&] { return it->second.done; });
                if (got_pong) {
                    result.received += 1;
                    result.rtt_ms.push_back(it->second.rtt_ms);
                }
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                pending_.erase(ping_id);
            }
        }

        if (!result.rtt_ms.empty()) {
            result.any_success = true;
            double sum = 0.0;
            result.min_ms = result.rtt_ms.front();
            result.max_ms = result.rtt_ms.front();
            for (double v : result.rtt_ms) {
                sum += v;
                if (v < result.min_ms) result.min_ms = v;
                if (v > result.max_ms) result.max_ms = v;
            }
            result.avg_ms = sum / static_cast<double>(result.rtt_ms.size());
        }
        return result;
    }

private:
    struct PendingPing {
        int slot = 0;
        std::chrono::steady_clock::time_point sent_at;
        bool done = false;
        double rtt_ms = 0.0;
    };

    uint64_t NextPingId()
    {
        return next_ping_id_++;
    }

    UdpSender& sender_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_map<uint64_t, PendingPing> pending_;
    uint64_t next_ping_id_ = 1;
};
