#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <boost/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <functional>
#include <algorithm>
#include <limits>
#include <cctype>

// One server-owned document. Persist before publishing, and serialize competing writers.
class OperationalContext {
public:
    explicit OperationalContext(std::string path = "data/operational_context.json") : path_(std::move(path)) {
        state_ = {{"context_version", 0}, {"active_uav_id", nullptr}, {"active_alert_id", nullptr},
            {"active_mission_id", nullptr}, {"active_security_plan_id", nullptr}, {"active_area_id", nullptr},
            {"operation_mode", "MONITOR"}, {"updated_at", 0}, {"updated_by", "Backend"}};
        if (!path_.empty() && std::filesystem::exists(path_)) {
            std::ifstream in(path_);
            std::string text((std::istreambuf_iterator<char>(in)), {});
            state_ = boost::json::parse(text).as_object(); // Corrupt persistence must fail visibly.
        }
    }
    boost::json::object snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_); return state_;
    }
    boost::json::object update(const boost::json::object& patch, const std::string& source,
                              const std::function<void(const boost::json::object&)>& publish) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto next = state_;
        boost::json::array changes;
        for (const auto& item : patch) {
            const std::string key(item.key());
            if (key != "active_uav_id" && key != "active_alert_id" && key != "active_mission_id" &&
                key != "active_security_plan_id" && key != "active_area_id" && key != "operation_mode")
                throw std::invalid_argument("unknown context field");
            if (!item.value().is_null() && !item.value().is_string()) throw std::invalid_argument("ID must be string or null");
            if (item.value().is_string() && (item.value().as_string().empty() || item.value().as_string().size() > 128))
                throw std::invalid_argument("invalid context value length");
            if (key == "active_uav_id" && !item.value().is_null()) {
                const std::string id(item.value().as_string());
                if (id.rfind("UAV-", 0) != 0 || id.size() <= 4 ||
                    !std::all_of(id.begin()+4, id.end(), [](unsigned char c){return std::isdigit(c);}))
                    throw std::invalid_argument("expected UAV numeric identifier");
                try { const auto number = std::stoll(id.substr(4));
                    if (number <= 0 || number > std::numeric_limits<int>::max()) throw std::invalid_argument("invalid UAV number");
                    const auto canonical = "UAV-" + std::string(number < 10 ? "0" : "") + std::to_string(number);
                    if (id != canonical) throw std::invalid_argument("noncanonical UAV identifier");
                } catch (...) { throw std::invalid_argument("invalid UAV identifier"); }
            }
            if (key == "operation_mode" && item.value() != "MONITOR" && item.value() != "PLAN_EDIT" &&
                item.value() != "MISSION_EXECUTION" && item.value() != "ALERT_RESPONSE")
                throw std::invalid_argument("invalid operation mode");
            if (next.at(key) != item.value()) {
                next[key] = item.value();
                changes.emplace_back(key == "operation_mode" ? "operation_mode_changed" : key.substr(0, key.size()-3) + "_changed");
            }
        }
        if (changes.empty()) return state_;
        next["context_version"] = state_.at("context_version").to_number<int64_t>() + 1;
        next["updated_at"] = now(); next["updated_by"] = source;
        persist(next);
        state_ = next;
        const auto version = state_.at("context_version");
        publish({{"type", "OperationalContextChanged"}, {"event_type", "OperationalContextChanged"},
            {"event_id", "context-" + std::to_string(version.to_number<int64_t>())}, {"version", version},
            {"timestamp", now()}, {"source_client", source}, {"changes", changes}, {"payload", state_}});
        return state_;
    }
    static double now() {
        return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
private:
    void persist(const boost::json::object& value) {
        if (path_.empty()) return;
        const std::filesystem::path target(path_);
        if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path());
        const auto temp = path_ + ".tmp";
        { std::ofstream out(temp, std::ios::trunc); out << boost::json::serialize(value); out.flush();
          if (!out) throw std::runtime_error("context persistence failed"); }
#ifdef _WIN32
        // MoveFileEx atomically replaces the previous document on Windows.
        if (!MoveFileExA(temp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("context persistence replace failed");
#else
        std::filesystem::rename(temp, path_);
#endif
    }
    std::string path_;
    mutable std::mutex mutex_;
    boost::json::object state_;
};
