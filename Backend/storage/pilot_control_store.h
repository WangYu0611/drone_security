#pragma once

#include <boost/json.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

struct PilotControlError : std::runtime_error {
    int status_code;
    std::string code;

    PilotControlError(int status, std::string code, const std::string& message);
};

// Mock control authority. The server serializes access with competing flight
// operations and applies Mock input under that same transaction lock.
class PilotControlStore {
public:
    enum class Role { Command, Pilot };

    struct Actor {
        Role role;
        std::string id;
    };

    struct Aircraft {
        std::string uav_id;
        std::string unavailable_reason;
        std::string video_url;
    };

    struct ActiveInput {
        std::string uav_id;
        double forward;
        double right;
        double vertical;
        double yaw;
    };

    using Connections = std::map<std::string, std::string>;
    using Clock = std::chrono::steady_clock;

    explicit PilotControlStore(std::filesystem::path path = {});

    boost::json::object Snapshot(const Actor& actor, const std::vector<Aircraft>& aircraft,
                                 const Connections& connections, const std::string& connection_id = {}) const;
    boost::json::object Apply(const Actor& actor, const boost::json::object& request,
                              const std::vector<Aircraft>& aircraft, const Connections& connections,
                              Clock::time_point now = Clock::now());
    bool Input(const Actor& actor, const std::string& connection_id,
               const boost::json::object& input, Clock::time_point now = Clock::now());
    boost::json::object InputWindow(const Actor& actor, const std::string& connection_id,
                                    Clock::time_point now = Clock::now());

    void RefreshView(const std::vector<Aircraft>& aircraft, const Connections& connections);
    void Disconnect(const std::string& connection_id);
    void DeactivateAircraft(const std::string& uav_id, const std::string& reason);
    void Expire(Clock::time_point now = Clock::now());
    std::vector<ActiveInput> ActiveInputs(Clock::time_point now = Clock::now()) const;
    bool OwnsAircraft(const std::string& uav_id) const;
    std::uint64_t Version() const { return state_.version; }

private:
    struct Grant {
        std::string id;
        std::string pilot_id;
    };

    struct Window {
        std::uint64_t id;
        Clock::time_point issued_at;
    };

    struct Session {
        std::string id;
        std::string connection_id;
        Clock::time_point input_deadline;
        std::optional<Window> current_window;
        std::optional<Window> previous_window;
        std::uint64_t last_sequence = 0;
        std::optional<ActiveInput> input;
        Clock::time_point input_valid_until;
    };

    struct Record {
        std::optional<Grant> grant;
        std::optional<Session> session;
        std::uint64_t revision = 0;
    };

    struct Receipt {
        boost::json::object request;
        boost::json::object response;
        int status = 200;
        std::string code;
        std::string message;
    };

    struct State {
        std::map<std::string, Record> aircraft;
        std::map<std::pair<std::string, std::string>, Receipt> receipts;
        std::map<std::string, boost::json::object> last_exit;
        std::uint64_t version = 0;
    };

    static void Change(State& state, Record& record);
    static void EndControl(State& state, const std::string& uav_id, Record& record,
                           const std::string& reason, bool revoke);
    static Record* FindSession(State& state, const Actor& actor, const std::string& connection_id);
    boost::json::array GrantAircraft(State& next, const boost::json::object& request,
                                     const std::vector<Aircraft>& aircraft, const Connections& connections);
    boost::json::object ChangeControl(State& next, const Actor& actor, const std::string& action,
                                     const boost::json::object& request, const std::vector<Aircraft>& aircraft,
                                     const Connections& connections, Clock::time_point now);
    void Load();
    void Persist(const State& state) const;

    std::filesystem::path path_;
    std::string server_epoch_;
    State state_;
    std::map<std::string, std::pair<std::string, std::string>> aircraft_view_;
    std::set<std::string> pilot_view_;
};
