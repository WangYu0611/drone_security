#pragma once

// debug_cli.h — 后端内置调试命令行（仅 debug 模式启用）
//
// 在独立线程中阻塞读取 stdin，解析简单命令并调用 DroneManager / WsManager。
// 不依赖任何外部工具，局域网离线环境直接可用。
//
// 用法（main.cpp）：
//   if (config.debug) {
//       auto cli = std::thread(RunDebugCli, std::ref(drone_mgr),
//                              std::ref(assembly_ctrl), std::ref(ws_manager),
//                              std::ref(g_running));
//       cli.detach();
//   }
//
// 支持的命令（均以 Enter 确认）：
//
//   help                          — 显示帮助
//   list                          — 列出已注册无人机
//   fresh                         — 重新探测所有断联无人机（单包发现或安全 hold）
//   inject <id> <n> <e> <d> [bat] [lat] [lon] [alt]
//                                 — 注入一条遥测（NED 米，可选电量和 GPS）
//   move <id> <ue_x> <ue_y> <ue_z>
//                                 — 发送 move 指令（UE 偏移，厘米）
//   pause <id>                    — 暂停指令队列
//   resume <id>                   — 恢复指令队列
//   state <id>                    — 打印无人机状态
//   ws_count                      — 打印当前 WebSocket 连接数
//   delay-test <id> [count]       — UDP 控制链路往返延迟测试（RTT，ping/pong）
//   quit / exit / q               — 退出后端

#include "communication/latency_tester.h"
#include "communication/ws_manager.h"
#include "drone/drone_manager.h"
#include "execution/assembly_controller.h"
#include "core/types.h"

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

// ---- 简单分词 ----
static std::vector<std::string> cli_split(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

static int cli_id(const std::string& s)
{
    // 接受 "1" 或 "d1"
    if (!s.empty() && s[0] == 'd') return std::stoi(s.substr(1));
    return std::stoi(s);
}

static void cli_print_help()
{
    std::cout <<
        "\n=== Debug CLI ===\n"
        "  help\n"
        "  list\n"
        "  fresh  (probe all offline/lost drones; discovery packet or safe hold)\n"
        "  inject <id> <N> <E> <D> [battery] [gps_lat] [gps_lon] [gps_alt]\n"
        "    id      : drone id, e.g. 1 or d1\n"
        "    N E D   : NED position (meters), D negative = above origin\n"
        "    battery : battery % (default -1 = unavailable)\n"
        "    gps_*   : provide on first inject to trigger power_on anchor\n"
        "  move <id> <ue_x> <ue_y> <ue_z>  (UE offset, cm)\n"
        "  pause  <id>\n"
        "  resume <id>\n"
        "  state  <id>\n"
        "  ws_count\n"
        "  delay-test <id> [count]  (UDP RTT ping/pong; default count=10)\n"
        "    只用后端自身时钟计算往返时延，不依赖 Jetson 系统时钟\n"
        "  quit / exit / q\n"
        "=================\n\n";
}

inline void RunDebugCli(DroneManager&       drone_mgr,
                        AssemblyController& assembly_ctrl,
                        WsManager&          ws_manager,
                        LatencyTester&      latency_tester,
                        std::atomic<bool>&  running)
{
    std::cout << "[DebugCLI] Started. Type 'help' for commands.\n";
    std::string line;

    while (running && std::getline(std::cin, line)) {
        if (!running) break;

        // 去首尾空白
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        const auto tokens = cli_split(line);
        if (tokens.empty()) continue;

        const std::string& cmd = tokens[0];

        try {
            // ---- quit ----
            if (cmd == "quit" || cmd == "exit" || cmd == "q") {
                running = false;
                std::cout << "[DebugCLI] Shutting down.\n";
                break;
            }

            // ---- help ----
            if (cmd == "help") {
                cli_print_help();
                continue;
            }

            // ---- fresh ----
            if (cmd == "fresh") {
                const auto refreshed_ids = drone_mgr.RefreshDisconnectedConnections();
                if (refreshed_ids.empty()) {
                    std::cout << "  [fresh] no offline/lost drones to probe\n";
                } else {
                    std::cout << "  [fresh] probe sent to:";
                    for (const int id : refreshed_ids) {
                        std::cout << " d" << id;
                    }
                    std::cout << "\n";
                }
                continue;
            }

            // ---- list ----
            if (cmd == "list") {
                const auto all = drone_mgr.GetAllStatus();
                if (all.empty()) {
                    std::cout << "  (no drones registered)\n";
                } else {
                    std::cout << "  ID    Slot  State             Bat   NED(m)\n";
                    for (const auto& s : all) {
                        std::cout << "  " << s.id
                                  << "     " << s.slot
                                  << "     " << s.connection_state
                                  << "     " << s.battery << "%"
                                  << "     (" << s.pos_x << ", " << s.pos_y << ", " << s.pos_z << ")\n";
                    }
                }
                continue;
            }

            // ---- ws_count ----
            if (cmd == "ws_count") {
                std::cout << "  WebSocket connections: " << ws_manager.count() << "\n";
                continue;
            }

            // ---- state <id> ----
            if (cmd == "state") {
                if (tokens.size() < 2) { std::cout << "usage: state <id>\n"; continue; }
                int id = cli_id(tokens[1]);
                if (!drone_mgr.HasDrone(id)) { std::cout << "  drone " << id << " not found\n"; continue; }
                auto s = drone_mgr.GetStatus(id);
                auto hb = drone_mgr.GetHeartbeatStats(id);
                auto anchor = drone_mgr.GetAnchor(id);
                std::cout
                    << "  ID=" << s.id << "  Slot=" << s.slot
                    << "  state=" << s.connection_state
                    << "  bat=" << s.battery << "%\n"
                    << "  NED=(" << s.pos_x << ", " << s.pos_y << ", " << s.pos_z << ")m"
                    << "  Yaw=" << s.yaw << "deg  Speed=" << s.speed << "m/s\n"
                    << "  heartbeat: running=" << hb.running << "  sent=" << hb.sent_count << "\n"
                    << "  GPS anchor: " << (anchor.valid ? "valid" : "none");
                if (anchor.valid) {
                    std::cout << "  lat=" << anchor.latitude
                              << "  lon=" << anchor.longitude
                              << "  alt=" << anchor.altitude;
                }
                std::cout << "\n";
                continue;
            }

            // ---- inject <id> <N> <E> <D> [bat] [lat] [lon] [alt] ----
            if (cmd == "inject") {
                if (tokens.size() < 5) {
                    std::cout << "usage: inject <id> <N> <E> <D> [battery] [gps_lat] [gps_lon] [gps_alt]\n";
                    continue;
                }
                int id = cli_id(tokens[1]);
                if (!drone_mgr.HasDrone(id)) { std::cout << "  drone " << id << " not found\n"; continue; }

                TelemetryData tel{};
                tel.position_ned[0] = std::stod(tokens[2]);
                tel.position_ned[1] = std::stod(tokens[3]);
                tel.position_ned[2] = std::stod(tokens[4]);
                tel.local_position[0] = tel.position_ned[0];
                tel.local_position[1] = tel.position_ned[1];
                tel.local_position[2] = tel.position_ned[2];
                tel.local_position_valid = true;
                tel.battery         = tokens.size() >= 6 ? std::stoi(tokens[5]) : -1;
                tel.gps_lat         = tokens.size() >= 7 ? std::stod(tokens[6]) : 0.0;
                tel.gps_lon         = tokens.size() >= 8 ? std::stod(tokens[7]) : 0.0;
                tel.gps_alt         = tokens.size() >= 9 ? std::stod(tokens[8]) : 0.0;
                tel.gps_fix         = (tokens.size() >= 7);  // 提供了 GPS 坐标就视为有 fix

                drone_mgr.OnTelemetryReceived(id, tel);

                if (assembly_ctrl.GetState() == AssemblyState::Assembling) {
                    assembly_ctrl.UpdateDronePosition(id,
                        tel.position_ned[0], tel.position_ned[1], tel.position_ned[2]);
                }

                std::cout << "  [inject] d" << id
                          << "  NED=(" << tel.position_ned[0]
                          << ", " << tel.position_ned[1]
                          << ", " << tel.position_ned[2] << ")m"
                          << "  bat=" << tel.battery << "%";
                if (tel.gps_fix)
                    std::cout << "  GPS=(" << tel.gps_lat << ", " << tel.gps_lon << ", " << tel.gps_alt << ")";
                std::cout << "\n";
                continue;
            }

            // ---- move <id> <ue_x> <ue_y> <ue_z> ----
            if (cmd == "move") {
                if (tokens.size() < 5) { std::cout << "usage: move <id> <ue_x> <ue_y> <ue_z>\n"; continue; }
                int id = cli_id(tokens[1]);
                double ux = std::stod(tokens[2]);
                double uy = std::stod(tokens[3]);
                double uz = std::stod(tokens[4]);
                if (!drone_mgr.ProcessMoveCommand(id, ux, uy, uz)) {
                    std::cout << "  drone " << id << " not found\n";
                } else {
                    std::cout << "  [move] d" << id << "  UE=(" << ux << ", " << uy << ", " << uz << ")cm\n";
                }
                continue;
            }

            // ---- pause <id> ----
            if (cmd == "pause") {
                if (tokens.size() < 2) { std::cout << "usage: pause <id>\n"; continue; }
                int id = cli_id(tokens[1]);
                drone_mgr.ProcessPauseCommand(id);
                std::cout << "  [pause] d" << id << "\n";
                continue;
            }

            // ---- resume <id> ----
            if (cmd == "resume") {
                if (tokens.size() < 2) { std::cout << "usage: resume <id>\n"; continue; }
                int id = cli_id(tokens[1]);
                drone_mgr.ProcessResumeCommand(id);
                std::cout << "  [resume] d" << id << "\n";
                continue;
            }

            // ---- delay-test <id> [count] ----
            if (cmd == "delay-test") {
                if (tokens.size() < 2) { std::cout << "usage: delay-test <id> [count]\n"; continue; }
                int id = cli_id(tokens[1]);
                if (!drone_mgr.HasDrone(id)) { std::cout << "  drone " << id << " not found\n"; continue; }
                int count = tokens.size() >= 3 ? std::stoi(tokens[2]) : 10;
                if (count < 1 || count > 1000) { std::cout << "  count must be 1..1000\n"; continue; }
                int slot = drone_mgr.GetStatus(id).slot;

                std::cout << "  [delay-test] d" << id << " (slot " << slot
                          << ") sending " << count << " probes...\n";
                auto result = latency_tester.RunTest(id, slot, count, /*timeout_ms=*/2000);

                std::cout << "  sent=" << result.sent << "  received=" << result.received
                          << "  loss=" << (result.sent - result.received) << "\n";
                if (result.any_success) {
                    std::cout << "  RTT(ms): min=" << result.min_ms
                              << "  avg=" << result.avg_ms
                              << "  max=" << result.max_ms << "\n"
                              << "  单向近似(若上下行对称)=" << (result.avg_ms / 2.0) << "ms\n";
                } else {
                    std::cout << "  no pong received; check Jetson bridge is running and reachable\n";
                }
                continue;
            }

            std::cout << "  unknown command: " << cmd << "  (type 'help')\n";

        } catch (const std::exception& e) {
            std::cout << "  [error] " << e.what() << "\n";
        }
    }
}
