#include "storage/pilot_control_store.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <random>
#include <sstream>
#include <utility>

namespace {
namespace json = boost::json;
using namespace std::chrono_literals;

// These values govern only the Mock channel. Hardware timing is configured
// and verified independently when a real control adapter exists.
constexpr auto mock_window_rotation = 250ms;
constexpr auto mock_window_lifetime = 500ms;
constexpr auto mock_stick_lifetime = 400ms;
constexpr auto mock_input_timeout = 750ms;
constexpr std::size_t max_pending_actions = 16;

std::string identifier() {
    std::random_device entropy;
    std::ostringstream value;
    value << std::hex << std::setfill('0');
    for (int part = 0; part < 4; ++part)
        value << std::setw(8) << static_cast<std::uint32_t>(entropy());
    return value.str();
}

std::string required_string(const json::object& object, const char* key) {
    const auto* value = object.if_contains(key);
    if (!value || !value->is_string() || value->as_string().empty() || value->as_string().size() > 128)
        throw PilotControlError(400, "INVALID_REQUEST", std::string("Invalid ") + key);
    return std::string(value->as_string());
}

std::optional<std::uint64_t> unsigned_integer(const json::value* value) {
    if (!value) return std::nullopt;
    if (value->is_uint64()) return value->as_uint64();
    if (value->is_int64() && value->as_int64() >= 0)
        return static_cast<std::uint64_t>(value->as_int64());
    return std::nullopt;
}

std::uint64_t required_revision(const json::value* value) {
    const auto revision = unsigned_integer(value);
    if (!revision) throw PilotControlError(400, "INVALID_REQUEST", "Invalid aircraft revision");
    return *revision;
}

std::string actor_key(const PilotControlStore::Actor& actor) {
    return (actor.role == PilotControlStore::Role::Command ? "Command:" : "Pilot:") + actor.id;
}

void check_request_fields(const json::object& request, const std::string& action) {
    for (const auto& item : request) {
        const auto key = item.key();
        if (key == "action" || key == "request_id") continue;
        if (action == "grant" && (key == "pilot_id" || key == "uav_ids" || key == "expected_revisions")) continue;
        if (action == "activate" && (key == "uav_id" || key == "grant_id" || key == "connection_id" || key == "expected_revision")) continue;
        if (action == "revoke" && (key == "uav_id" || key == "grant_id")) continue;
        if (action == "stop" && (key == "control_session_id" || key == "reason")) continue;
        throw PilotControlError(400, "INVALID_REQUEST", "Unexpected pilot control request field");
    }
}

const PilotControlStore::Aircraft* find_aircraft(const std::vector<PilotControlStore::Aircraft>& aircraft,
                                               const std::string& uav_id) {
    const auto it = std::find_if(aircraft.begin(), aircraft.end(), [&](const auto& entry) {
        return entry.uav_id == uav_id;
    });
    return it == aircraft.end() ? nullptr : &*it;
}

std::set<std::string> online_pilots(const PilotControlStore::Connections& connections) {
    std::set<std::string> pilots;
    for (const auto& connection : connections) pilots.insert(connection.second);
    return pilots;
}

std::optional<double> axis(const json::object& axes, const char* key) {
    const auto* value = axes.if_contains(key);
    if (!value || !value->is_number()) return std::nullopt;
    const auto number = value->to_number<double>();
    if (!std::isfinite(number) || number < -1 || number > 1) return std::nullopt;
    return number;
}

std::optional<PilotControlStore::Action> flight_action(const json::value* value) {
    using Action = PilotControlStore::Action;
    if (!value) return Action::None;
    if (!value->is_string()) return std::nullopt;
    const auto& name = value->as_string();
    if (name == "none") return Action::None;
    if (name == "takeoff") return Action::Takeoff;
    if (name == "land") return Action::Land;
    if (name == "hold") return Action::Hold;
    if (name == "gohome") return Action::GoHome;
    if (name == "estop") return Action::EmergencyStop;
    return std::nullopt;
}

json::object result(const std::string& uav_id, bool ok, const std::string& code, std::uint64_t revision) {
    return {{"uav_id", uav_id}, {"ok", ok}, {"code", code}, {"revision", revision}};
}
}

PilotControlError::PilotControlError(int status, std::string error_code, const std::string& message)
    : std::runtime_error(message), status_code(status), code(std::move(error_code)) {}

PilotControlStore::PilotControlStore(std::filesystem::path path)
    : path_(std::move(path)), server_epoch_(identifier()) {
    Load();
}

void PilotControlStore::Change(State& state, Record& record) {
    ++record.revision;
    ++state.version;
}

void PilotControlStore::EndControl(State& state, const std::string& uav_id, Record& record,
                                  const std::string& reason, bool revoke) {
    const auto pilot_id = record.grant->pilot_id;
    record.session.reset();
    if (revoke) record.grant.reset();
    Change(state, record);
    // The Mock simulator consumes input under the same transaction lock and
    // has no independently queued position commands to acknowledge.
    state.last_exit[pilot_id] = {{"uav_id", uav_id}, {"reason", reason},
                                {"revision", record.revision}, {"aircraft_state", "CONFIRMED"}};
}

bool PilotControlStore::OwnsAircraft(const std::string& uav_id) const {
    const auto it = state_.aircraft.find(uav_id);
    return it != state_.aircraft.end() && it->second.grant.has_value();
}

void PilotControlStore::RefreshView(const std::vector<Aircraft>& aircraft, const Connections& connections) {
    decltype(aircraft_view_) aircraft_view;
    for (const auto& entry : aircraft)
        aircraft_view.emplace(entry.uav_id, std::make_pair(entry.unavailable_reason, entry.video_url));
    auto pilots = online_pilots(connections);
    if (aircraft_view_ == aircraft_view && pilot_view_ == pilots) return;
    for (const auto& entry : aircraft_view_) {
        const auto current = aircraft_view.find(entry.first);
        if (current == aircraft_view.end() || current->second.first != entry.second.first)
            ++state_.aircraft[entry.first].revision;
    }
    for (auto& entry : state_.aircraft) {
        if (!entry.second.session) continue;
        const auto current = aircraft_view.find(entry.first);
        if (current == aircraft_view.end()) {
            EndControl(state_, entry.first, entry.second, "UAV_NOT_AVAILABLE", false);
        } else if (!current->second.first.empty()) {
            EndControl(state_, entry.first, entry.second, current->second.first, false);
        }
    }
    aircraft_view_ = std::move(aircraft_view);
    pilot_view_ = std::move(pilots);
    ++state_.version;
}

json::object PilotControlStore::Snapshot(const Actor& actor, const std::vector<Aircraft>& aircraft,
                                         const Connections& connections, const std::string& connection_id) const {
    std::map<std::string, const Aircraft*> fleet;
    for (const auto& entry : aircraft) fleet.emplace(entry.uav_id, &entry);
    for (const auto& entry : state_.aircraft)
        if (entry.second.grant) fleet.emplace(entry.first, nullptr);

    const bool command = actor.role == Role::Command;
    const auto pilots_online = online_pilots(connections);
    auto known_pilots = pilots_online;
    json::array rows;
    for (const auto& entry : fleet) {
        const auto& uav_id = entry.first;
        const auto stored = state_.aircraft.find(uav_id);
        const Record empty;
        const auto& record = stored == state_.aircraft.end() ? empty : stored->second;
        if (record.grant && command) known_pilots.insert(record.grant->pilot_id);
        if (!command && (!record.grant || record.grant->pilot_id != actor.id)) continue;

        const auto* plane = entry.second;
        const auto unavailable = plane ? plane->unavailable_reason : "UAV_NOT_AVAILABLE";
        const auto reason = unavailable.empty() && record.grant ? "UAV_ALREADY_ASSIGNED" : unavailable;
        json::object row{{"uav_id", uav_id}, {"revision", record.revision},
                         {"assignment_state", record.grant ? "GRANTED" : "UNASSIGNED"},
                         {"control_state", record.session ? "ACTIVE" : "IDLE"},
                         {"input_frozen", record.session && record.session->emergency_stopped},
                         {"available", reason.empty()}, {"reason", reason},
                         {"aircraft_state", "CONFIRMED"}, {"pilot_id", nullptr}, {"grant_id", nullptr},
                         {"video_url", plane ? plane->video_url : std::string{}}};
        if (record.grant) {
            row["pilot_id"] = record.grant->pilot_id;
            row["grant_id"] = record.grant->id;
        }
        if (!command) {
            row["activation_reason"] = unavailable;
            if (record.session && record.session->connection_id == connection_id)
                row["control_session_id"] = record.session->id;
        }
        rows.emplace_back(std::move(row));
    }

    json::array pilots;
    if (command) {
        for (const auto& pilot_id : known_pilots)
            pilots.emplace_back(json::object{{"pilot_id", pilot_id}, {"online", pilots_online.count(pilot_id) != 0}});
    }
    json::object snapshot{{"server_epoch", server_epoch_}, {"version", state_.version},
                          {"aircraft", std::move(rows)}, {"pilots", std::move(pilots)}};
    if (!command) {
        const auto exit = state_.last_exit.find(actor.id);
        if (exit != state_.last_exit.end()) snapshot["last_exit"] = exit->second;
    }
    return snapshot;
}

json::array PilotControlStore::GrantAircraft(State& next, const json::object& request,
                                            const std::vector<Aircraft>& aircraft,
                                            const Connections& connections) {
    const auto pilot_id = required_string(request, "pilot_id");
    const auto* ids = request.if_contains("uav_ids");
    const auto* revisions = request.if_contains("expected_revisions");
    if (!ids || !ids->is_array() || ids->as_array().empty() || ids->as_array().size() > 64 ||
        !revisions || !revisions->is_object() || ids->as_array().size() != revisions->as_object().size())
        throw PilotControlError(400, "INVALID_REQUEST", "Expected one to 64 aircraft and their revisions");

    std::set<std::string> visited;
    const auto online = online_pilots(connections).count(pilot_id) != 0;
    json::array results;
    for (const auto& value : ids->as_array()) {
        if (!value.is_string() || value.as_string().empty() || value.as_string().size() > 128)
            throw PilotControlError(400, "INVALID_REQUEST", "Invalid aircraft identifier");
        const std::string id(value.as_string());
        if (!visited.insert(id).second)
            throw PilotControlError(400, "INVALID_REQUEST", "Duplicate aircraft identifier");
        const auto expected = required_revision(revisions->as_object().if_contains(id));
        auto& record = next.aircraft[id];
        const auto* plane = find_aircraft(aircraft, id);
        if (!plane) {
            results.emplace_back(result(id, false, "UAV_NOT_AVAILABLE", record.revision));
            continue;
        }
        if (expected != record.revision) {
            results.emplace_back(result(id, false, "STALE_STATE", record.revision));
            continue;
        }
        if (record.grant) {
            const auto same_pilot = record.grant->pilot_id == pilot_id;
            auto row = result(id, same_pilot, same_pilot ? "ALREADY_GRANTED" : "UAV_ALREADY_ASSIGNED", record.revision);
            if (same_pilot) row["grant_id"] = record.grant->id;
            results.emplace_back(std::move(row));
            continue;
        }
        if (!online || !plane->unavailable_reason.empty()) {
            results.emplace_back(result(id, false, online ? plane->unavailable_reason : "PILOT_OFFLINE", record.revision));
            continue;
        }
        record.grant = Grant{identifier(), pilot_id};
        Change(next, record);
        auto row = result(id, true, "GRANTED", record.revision);
        row["grant_id"] = record.grant->id;
        results.emplace_back(std::move(row));
    }
    return results;
}

json::object PilotControlStore::ChangeControl(State& next, const Actor& actor, const std::string& action,
                                             const json::object& request, const std::vector<Aircraft>& aircraft,
                                             const Connections& connections, Clock::time_point now) {
    if (action == "stop") {
        const auto session_id = required_string(request, "control_session_id");
        const auto reason = required_string(request, "reason");
        for (auto& entry : next.aircraft) {
            auto& record = entry.second;
            if (!record.session || record.session->id != session_id) continue;
            if (record.grant->pilot_id != actor.id)
                throw PilotControlError(403, "ROLE_FORBIDDEN", "The pilot does not own this session");
            EndControl(next, entry.first, record, reason, false);
            return result(entry.first, true, "STOPPED", record.revision);
        }
        throw PilotControlError(409, "SESSION_EXPIRED", "The control session is inactive");
    }

    const auto uav_id = required_string(request, "uav_id");
    const auto grant_id = required_string(request, "grant_id");
    const auto entry = next.aircraft.find(uav_id);
    if (entry == next.aircraft.end() || !entry->second.grant)
        throw PilotControlError(409, "GRANT_CHANGED", "The aircraft grant is inactive");
    auto& record = entry->second;
    if (action == "activate" && record.grant->pilot_id != actor.id)
        throw PilotControlError(403, "ROLE_FORBIDDEN", "The pilot does not own this grant");
    if (record.grant->id != grant_id)
        throw PilotControlError(409, "GRANT_CHANGED", "The aircraft grant has changed");
    if (action == "revoke") {
        EndControl(next, uav_id, record, "GRANT_REVOKED", true);
        return result(uav_id, true, "REVOKED", record.revision);
    }

    const auto expected = required_revision(request.if_contains("expected_revision"));
    if (expected != record.revision)
        throw PilotControlError(409, "STALE_STATE", "The aircraft state has changed");
    const auto connection_id = required_string(request, "connection_id");
    const auto connection = connections.find(connection_id);
    if (connection == connections.end() || connection->second != actor.id)
        throw PilotControlError(409, "PILOT_OFFLINE", "This pilot connection is not current");
    if (std::any_of(next.aircraft.begin(), next.aircraft.end(), [&](const auto& candidate) {
        const auto& other = candidate.second;
        return other.session && other.grant->pilot_id == actor.id;
    })) throw PilotControlError(409, "PILOT_BUSY", "The pilot already controls an aircraft");

    const auto* plane = find_aircraft(aircraft, uav_id);
    if (!plane || !plane->unavailable_reason.empty())
        throw PilotControlError(409, plane ? plane->unavailable_reason : "UAV_NOT_AVAILABLE", "The aircraft is unavailable");

    Session session;
    session.id = identifier();
    session.connection_id = connection_id;
    session.input_deadline = now + mock_input_timeout;
    record.session = std::move(session);
    next.last_exit.erase(actor.id);
    Change(next, record);
    return result(uav_id, true, "ACTIVE", record.revision);
}

json::object PilotControlStore::Apply(const Actor& actor, const json::object& request,
                                      const std::vector<Aircraft>& aircraft, const Connections& connections,
                                      Clock::time_point now) {
    const auto action = required_string(request, "action");
    const bool command_action = action == "grant" || action == "revoke";
    if (!command_action && action != "activate" && action != "stop")
        throw PilotControlError(400, "INVALID_REQUEST", "Unknown pilot control action");
    if (command_action != (actor.role == Role::Command))
        throw PilotControlError(403, "ROLE_FORBIDDEN", "This identity cannot perform the action");

    check_request_fields(request, action);
    const auto request_id = required_string(request, "request_id");
    const auto key = std::make_pair(actor_key(actor), request_id);
    const auto old = state_.receipts.find(key);
    if (old != state_.receipts.end()) {
        const auto& receipt = old->second;
        if (receipt.request != request)
            throw PilotControlError(409, "REQUEST_ID_REUSED", "The request ID already has different parameters");
        if (receipt.status != 200) throw PilotControlError(receipt.status, receipt.code, receipt.message);
        return receipt.response;
    }

    State next = state_;
    json::array results;
    try {
        if (action == "grant") results = GrantAircraft(next, request, aircraft, connections);
        else results.emplace_back(ChangeControl(next, actor, action, request, aircraft, connections, now));
    } catch (const PilotControlError& error) {
        if (error.status_code == 409) {
            next.receipts.emplace(key, Receipt{request, {}, error.status_code, error.code, error.what()});
            Persist(next);
            state_.receipts.swap(next.receipts);
        }
        throw;
    }

    json::object response{{"request_id", request_id}, {"server_epoch", server_epoch_},
                          {"version", next.version}, {"results", std::move(results)}};
    next.receipts.emplace(key, Receipt{request, response});
    try {
        Persist(next);
    } catch (...) {
        // Preserve the durable grant after an I/O failure, while ensuring an
        // operator's validated stop or revoke stops Mock input immediately.
        if (action == "stop" || action == "revoke") {
            const auto& row = response.at("results").as_array().front().as_object();
            DeactivateAircraft(std::string(row.at("uav_id").as_string()), "STATE_WRITE_FAILED");
        }
        throw;
    }
    state_ = std::move(next);
    return response;
}

PilotControlStore::Record* PilotControlStore::FindSession(State& state, const Actor& actor,
                                                         const std::string& connection_id) {
    if (actor.role != Role::Pilot || connection_id.empty()) return nullptr;
    for (auto& item : state.aircraft) {
        auto& record = item.second;
        if (record.session && record.session->connection_id == connection_id && record.grant->pilot_id == actor.id)
            return &record;
    }
    return nullptr;
}

json::object PilotControlStore::InputWindow(const Actor& actor, const std::string& connection_id,
                                            Clock::time_point now) {
    auto* record = FindSession(state_, actor, connection_id);
    if (!record || now >= record->session->input_deadline) return {};
    auto& session = *record->session;
    if (!session.current_window || now - session.current_window->issued_at >= mock_window_rotation) {
        session.previous_window = session.current_window;
        session.current_window = Window{session.previous_window ? session.previous_window->id + 1 : 1, now};
    }
    return {{"type", "pilot_input_window"}, {"control_session_id", session.id}, {"window_id", session.current_window->id}};
}

bool PilotControlStore::Input(const Actor& actor, const std::string& connection_id, const json::object& input,
                              Clock::time_point now) {
    if (actor.role != Role::Pilot || connection_id.empty()) return false;
    const auto* id = input.if_contains("uav_id");
    const auto* control_id = input.if_contains("control_session_id");
    const auto* axes = input.if_contains("axes");
    const auto sequence = unsigned_integer(input.if_contains("seq"));
    const auto window_id = unsigned_integer(input.if_contains("window_id"));
    if (!id || !id->is_string() || !control_id || !control_id->is_string() || !axes || !axes->is_object() ||
        !sequence || !window_id) return false;

    const auto record = state_.aircraft.find(std::string(id->as_string()));
    if (record == state_.aircraft.end() || !record->second.session || record->second.grant->pilot_id != actor.id)
        return false;
    auto& session = *record->second.session;
    if (session.connection_id != connection_id || session.id != control_id->as_string() ||
        *sequence <= session.last_sequence || now >= session.input_deadline) return false;

    const auto window_is_valid = [&](const std::optional<Window>& window) {
        return window && window->id == *window_id && now >= window->issued_at &&
               now - window->issued_at < mock_window_lifetime;
    };
    const auto& window = window_is_valid(session.current_window) ? session.current_window : session.previous_window;
    if (!window_is_valid(window)) return false;

    const auto& axes_object = axes->as_object();
    const auto forward = axis(axes_object, "forward");
    const auto right = axis(axes_object, "right");
    const auto vertical = axis(axes_object, "vertical");
    const auto yaw = axis(axes_object, "yaw");
    const auto action = flight_action(input.if_contains("action"));
    if (!forward || !right || !vertical || !yaw || !action) return false;

    const auto valid_until = std::min(now + mock_stick_lifetime, window->issued_at + mock_window_lifetime);
    ActiveInput next{record->first, 0., 0., 0., 0., Action::None, session.id};
    if (!session.emergency_stopped) {
        if (*action == Action::EmergencyStop) {
            session.pending_actions.clear();
            session.emergency_stopped = true;
            session.emergency_pending = true;
            Change(state_, record->second);
        } else if (*action != Action::None) {
            if (session.pending_actions.size() >= max_pending_actions) return false;
            session.pending_actions.push_back({*action, valid_until});
        } else {
            next.forward = *forward;
            next.right = *right;
            next.vertical = *vertical;
            next.yaw = *yaw;
        }
    }

    session.last_sequence = *sequence;
    session.input_deadline = now + mock_input_timeout;
    session.input = std::move(next);
    session.input_valid_until = valid_until;
    return true;
}

void PilotControlStore::Disconnect(const std::string& connection_id) {
    for (auto& entry : state_.aircraft)
        if (entry.second.session && entry.second.session->connection_id == connection_id)
            EndControl(state_, entry.first, entry.second, "LINK_LOST", false);
}

void PilotControlStore::DeactivateAircraft(const std::string& uav_id, const std::string& reason) {
    const auto it = state_.aircraft.find(uav_id);
    if (it != state_.aircraft.end() && it->second.session)
        EndControl(state_, uav_id, it->second, reason, false);
}

void PilotControlStore::Expire(Clock::time_point now) {
    for (auto& entry : state_.aircraft)
        if (entry.second.session && now >= entry.second.session->input_deadline)
            EndControl(state_, entry.first, entry.second, "INPUT_TIMEOUT", false);
}

std::vector<PilotControlStore::ActiveInput> PilotControlStore::ActiveInputs(Clock::time_point now) {
    std::vector<ActiveInput> active;
    for (auto& entry : state_.aircraft) {
        auto& session = entry.second.session;
        if (!session || now >= session->input_deadline) continue;

        if (session->emergency_pending) {
            active.push_back({entry.first, 0., 0., 0., 0., Action::EmergencyStop, session->id});
            session->emergency_pending = false;
            continue;
        }

        auto& pending = session->pending_actions;
        while (!pending.empty() && now >= pending.front().valid_until) pending.pop_front();
        if (!pending.empty()) {
            active.push_back({entry.first, 0., 0., 0., 0., pending.front().action, session->id});
            pending.pop_front();
            // Manual movement requires a new stick sample after the action is delivered.
            auto& sticks = *session->input;
            sticks.forward = 0.;
            sticks.right = 0.;
            sticks.vertical = 0.;
            sticks.yaw = 0.;
        } else if (session->input && now < session->input_valid_until) {
            active.push_back(*session->input);
        }
    }
    return active;
}

void PilotControlStore::Load() {
    if (path_.empty() || !std::filesystem::exists(path_)) return;
    std::ifstream input(path_);
    if (!input) throw std::runtime_error("Cannot read pilot control state");
    const std::string contents((std::istreambuf_iterator<char>(input)), {});
    const auto document = json::parse(contents).as_object();
    if (document.at("schema_version").to_number<int>() != 1)
        throw std::runtime_error("Unsupported pilot control state version");
    for (const auto& value : document.at("aircraft").as_array()) {
        const auto& row = value.as_object();
        auto& record = state_.aircraft[std::string(row.at("uav_id").as_string())];
        record.revision = row.at("revision").to_number<std::uint64_t>();
        if (!row.at("grant_id").is_null())
            record.grant = Grant{std::string(row.at("grant_id").as_string()), std::string(row.at("pilot_id").as_string())};
    }
    for (const auto& value : document.at("receipts").as_array()) {
        const auto& row = value.as_object();
        const auto key = std::make_pair(std::string(row.at("actor").as_string()), std::string(row.at("request_id").as_string()));
        state_.receipts.emplace(key, Receipt{row.at("request").as_object(), row.at("response").as_object(),
            row.at("status").to_number<int>(), std::string(row.at("code").as_string()), std::string(row.at("message").as_string())});
    }
}

void PilotControlStore::Persist(const State& state) const {
    if (path_.empty()) return;
    json::array aircraft;
    for (const auto& value : state.aircraft) {
        const auto& record = value.second;
        json::object row{{"uav_id", value.first}, {"revision", record.revision}, {"grant_id", nullptr}, {"pilot_id", nullptr}};
        if (record.grant) {
            row["grant_id"] = record.grant->id;
            row["pilot_id"] = record.grant->pilot_id;
        }
        aircraft.emplace_back(std::move(row));
    }
    json::array receipts;
    for (const auto& value : state.receipts) {
        const auto& receipt = value.second;
        receipts.emplace_back(json::object{{"actor", value.first.first}, {"request_id", value.first.second},
            {"request", receipt.request}, {"response", receipt.response}, {"status", receipt.status},
            {"code", receipt.code}, {"message", receipt.message}});
    }
    if (path_.has_parent_path()) std::filesystem::create_directories(path_.parent_path());
    auto temporary = path_;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    output << json::serialize(json::object{{"schema_version", 1}, {"aircraft", std::move(aircraft)}, {"receipts", std::move(receipts)}});
    output.close();
    if (!output) throw std::runtime_error("Cannot write pilot control state");
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace pilot control state");
#else
    std::filesystem::rename(temporary, path_);
#endif
}
