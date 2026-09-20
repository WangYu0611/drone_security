#include <gtest/gtest.h>
#include "drone/command_queue.h"
#include <thread>

TEST(CommandQueueTest, PushAndPop)
{
    CommandQueue q(128);

    DroneControlPacket cmd;
    cmd.timestamp = 1000.0;
    cmd.x = 1.0f;
    cmd.y = 2.0f;
    cmd.z = 3.0f;
    cmd.mode = 1;

    q.Push(cmd);
    EXPECT_EQ(q.Size(), 1);

    DroneControlPacket out;
    EXPECT_TRUE(q.Pop(out));
    EXPECT_DOUBLE_EQ(out.timestamp, 1000.0);
    EXPECT_FLOAT_EQ(out.x, 1.0f);
    EXPECT_EQ(q.Size(), 0);
}

TEST(CommandQueueTest, PopFromEmpty)
{
    CommandQueue q(128);
    DroneControlPacket out;
    EXPECT_FALSE(q.Pop(out));
}

TEST(CommandQueueTest, FifoOrder)
{
    CommandQueue q(128);

    DroneControlPacket cmd1, cmd2;
    cmd1.timestamp = 1.0;
    cmd2.timestamp = 2.0;

    q.Push(cmd1);
    q.Push(cmd2);

    DroneControlPacket out;
    q.Pop(out);
    EXPECT_DOUBLE_EQ(out.timestamp, 1.0);
    q.Pop(out);
    EXPECT_DOUBLE_EQ(out.timestamp, 2.0);
}

TEST(CommandQueueTest, CoalescesPendingVerticalTargets)
{
    CommandQueue q(128);

    DroneControlPacket first;
    first.sequence = 101;
    first.x = 12.0f;
    first.y = -4.0f;
    first.z = -2.0f;
    first.mode = 1;

    DroneControlPacket latest = first;
    latest.sequence = 102;
    latest.z = -8.0f;

    q.PushLatestVerticalTarget(first);
    q.PushLatestVerticalTarget(latest);

    ASSERT_EQ(q.Size(), 1);
    const auto pending = q.Snapshot();
    ASSERT_EQ(pending.size(), 1);
    EXPECT_EQ(pending.front().sequence, 102);
    EXPECT_FLOAT_EQ(pending.front().x, 12.0f);
    EXPECT_FLOAT_EQ(pending.front().y, -4.0f);
    EXPECT_FLOAT_EQ(pending.front().z, -8.0f);
}

TEST(CommandQueueTest, DoesNotCoalesceDifferentHorizontalTargets)
{
    CommandQueue q(128);

    DroneControlPacket first;
    first.x = 12.0f;
    first.y = -4.0f;
    first.z = -2.0f;
    first.mode = 1;

    DroneControlPacket different_horizontal_target = first;
    different_horizontal_target.x = 12.5f;
    different_horizontal_target.z = -8.0f;

    q.PushLatestVerticalTarget(first);
    q.PushLatestVerticalTarget(different_horizontal_target);

    EXPECT_EQ(q.Size(), 2);
}

TEST(CommandQueueTest, CoalescesPendingManualTargetsToLatest)
{
    CommandQueue q(128);

    DroneControlPacket first;
    first.sequence = 201;
    first.x = 1.0f;
    first.y = 2.0f;
    first.z = -3.0f;
    first.mode = 1;

    DroneControlPacket latest = first;
    latest.sequence = 202;
    latest.x = 4.0f;
    latest.y = 5.0f;
    latest.z = -6.0f;

    q.PushLatestManualTarget(first);
    q.PushLatestManualTarget(latest);

    ASSERT_EQ(q.Size(), 1);
    DroneControlPacket out;
    ASSERT_TRUE(q.Pop(out));
    EXPECT_EQ(out.sequence, 202);
    EXPECT_FLOAT_EQ(out.x, 4.0f);
    EXPECT_FLOAT_EQ(out.y, 5.0f);
    EXPECT_FLOAT_EQ(out.z, -6.0f);
}

TEST(CommandQueueTest, PreservesRequestedSpeedWhenReplacingManualTarget)
{
    CommandQueue q(128);

    DroneControlPacket first;
    first.sequence = 301;
    first.mode = 1;
    first.speed_mps = 1.5f;

    DroneControlPacket latest = first;
    latest.sequence = 302;
    latest.speed_mps = 3.25f;

    q.PushLatestManualTarget(first);
    q.PushLatestManualTarget(latest);

    DroneControlPacket out;
    ASSERT_TRUE(q.Pop(out));
    EXPECT_EQ(out.sequence, 302);
    EXPECT_FLOAT_EQ(out.speed_mps, 3.25f);
}

TEST(CommandQueueTest, OverflowDropsOldest)
{
    CommandQueue q(2);  // 最多 2 个

    DroneControlPacket cmd;
    cmd.timestamp = 1.0; q.Push(cmd);
    cmd.timestamp = 2.0; q.Push(cmd);
    cmd.timestamp = 3.0; q.Push(cmd);  // 应丢弃 1.0

    EXPECT_EQ(q.Size(), 2);

    DroneControlPacket out;
    q.Pop(out);
    EXPECT_DOUBLE_EQ(out.timestamp, 2.0);  // 最早的 1.0 被丢弃
}

TEST(CommandQueueTest, PauseBlocksPop)
{
    CommandQueue q(128);

    DroneControlPacket cmd;
    cmd.timestamp = 1.0;
    q.Push(cmd);

    q.SetPaused(true);
    DroneControlPacket out;
    EXPECT_FALSE(q.Pop(out));  // 暂停时无法出队
    EXPECT_EQ(q.Size(), 1);

    q.SetPaused(false);
    EXPECT_TRUE(q.Pop(out));   // 恢复后可出队
}

TEST(CommandQueueTest, Clear)
{
    CommandQueue q(128);
    q.Push(DroneControlPacket{});
    q.Push(DroneControlPacket{});
    EXPECT_EQ(q.Size(), 2);

    q.Clear();
    EXPECT_EQ(q.Size(), 0);
}

TEST(CommandQueueTest, ThreadSafety)
{
    constexpr int N = 10000;
    CommandQueue q(N);

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            DroneControlPacket cmd;
            cmd.timestamp = i;
            q.Push(cmd);
        }
    });

    std::thread consumer([&]() {
        int count = 0;
        DroneControlPacket out;
        while (count < N) {
            if (q.Pop(out)) ++count;
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(q.Size(), 0);
}
