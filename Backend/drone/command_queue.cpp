#include "command_queue.h"
#include <spdlog/spdlog.h>
#include <cmath>

namespace {
constexpr float kHorizontalTargetMergeEpsilonMeters = 0.01f;

bool HasSameHorizontalTarget(const DroneControlPacket& lhs, const DroneControlPacket& rhs)
{
    return lhs.mode == rhs.mode
        && std::fabs(lhs.x - rhs.x) <= kHorizontalTargetMergeEpsilonMeters
        && std::fabs(lhs.y - rhs.y) <= kHorizontalTargetMergeEpsilonMeters;
}
}
CommandQueue::CommandQueue(size_t max_size)
    : max_size_(max_size)
{
}

void CommandQueue::Push(const DroneControlPacket& cmd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.size() >= max_size_) {
        // 队列满，丢弃最旧指令
        queue_.pop();
        spdlog::warn("[CommandQueue] Queue full, dropping oldest command");
    }

    queue_.push(cmd);
}

void CommandQueue::PushLatestVerticalTarget(const DroneControlPacket& cmd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // std::queue::back() is the newest command that has not been claimed by
    // the heartbeat worker. Replacing it is safe because both commands keep
    // the same horizontal target and only the latest requested height matters.
    if (!queue_.empty() && HasSameHorizontalTarget(queue_.back(), cmd)) {
        const float supersededHeight = queue_.back().z;
        queue_.back() = cmd;
        spdlog::debug(
            "[CommandQueue] Coalesced pending vertical target: N/E=({:.3f},{:.3f}), D {:.3f}->{:.3f}",
            cmd.x, cmd.y, supersededHeight, cmd.z);
        return;
    }

    if (queue_.size() >= max_size_) {
        queue_.pop();
        spdlog::warn("[CommandQueue] Queue full, dropping oldest command");
    }

    queue_.push(cmd);
}

void CommandQueue::PushLatestManualTarget(const DroneControlPacket& cmd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // A manual target is an absolute, continuously updated endpoint. Drop all
    // unclaimed route/manual targets and retain only this endpoint; otherwise a
    // queued path can make live video control respond seconds too late.
    std::queue<DroneControlPacket> empty;
    queue_.swap(empty);
    queue_.push(cmd);
    spdlog::debug(
        "[CommandQueue] Replaced pending targets with manual target: N/E/D=({:.3f},{:.3f},{:.3f})",
        cmd.x, cmd.y, cmd.z);
}

bool CommandQueue::Pop(DroneControlPacket& cmd)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty() || paused_) {
        return false;
    }

    cmd = queue_.front();
    queue_.pop();
    return true;
}

bool CommandQueue::Peek(DroneControlPacket& cmd) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (queue_.empty()) {
        return false;
    }

    cmd = queue_.front();
    return true;
}

void CommandQueue::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) queue_.pop();
}

size_t CommandQueue::Size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

std::vector<DroneControlPacket> CommandQueue::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<DroneControlPacket> copy = queue_;
    std::vector<DroneControlPacket> result;
    result.reserve(copy.size());
    while (!copy.empty()) {
        result.push_back(copy.front());
        copy.pop();
    }
    return result;
}

void CommandQueue::SetPaused(bool paused)
{
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = paused;
    spdlog::info("[CommandQueue] {}", paused ? "Paused" : "Resumed");
}

bool CommandQueue::IsPaused() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return paused_;
}
