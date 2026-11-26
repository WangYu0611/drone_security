// Mock-only control sockets are never registered with the general WsManager.
#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>

struct HttpServer::PilotSocket : std::enable_shared_from_this<PilotSocket> {
    enum class Message { Subscribed, State, Window, Error, Count };

    PilotSocket(HttpServer& owner, tcp::socket socket,
                PilotControlStore::Actor actor, std::string connection_id)
        : owner(owner), actor(std::move(actor)), id(std::move(connection_id)),
          executor(socket.get_executor()), ws(std::move(socket)) {}

    void Run(const http::request<http::string_body>& request) {
        try {
            ws.set_option(ws_ns::stream_base::timeout::suggested(beast::role_type::server));
            ws.read_message_max(16 * 1024);
            ws.async_accept(request, [self = shared_from_this()](beast::error_code error) {
                if (error) self->Finish();
                else self->Read();
            });
            static_cast<net::io_context&>(executor.context()).run();
        } catch (...) {
            Finish();
            throw;
        }
    }

    void Enqueue(Message kind, const boost::json::object& payload) {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        if (mailbox_closed) return;
        pending[static_cast<std::size_t>(kind)] = json_stringify(payload);
        if (writing) return;
        writing = true;
        net::post(executor, [self = shared_from_this()] { self->Write(); });
    }

    void ClearWindow() {
        std::lock_guard<std::mutex> lock(mailbox_mutex);
        pending[static_cast<std::size_t>(Message::Window)].reset();
    }

    void Close() {
        net::post(executor, [self = shared_from_this()] { self->Finish(); });
    }

    HttpServer& owner;
    const PilotControlStore::Actor actor;
    const std::string id;
    // Subscription and published revisions are guarded by plan_requests_mutex_.
    bool subscribed = false;
    std::optional<uint64_t> published_version;
    boost::json::object last_window;

private:
    void Read() {
        if (finished) return;
        ws.async_read(received, [self = shared_from_this()](beast::error_code error, std::size_t) {
            if (error) {
                self->Finish();
                return;
            }
            try {
                beast::error_code parse_error;
                const auto value = boost::json::parse(beast::buffers_to_string(self->received.data()), parse_error);
                if (parse_error || !value.is_object() || !self->ws.got_text()) {
                    self->Error("INVALID_REQUEST", "Control messages must be JSON objects");
                } else {
                    self->Receive(value.as_object());
                }
            } catch (const PilotControlError& error) {
                self->Error(error.code, error.what());
            } catch (const std::exception&) {
                self->Error("PILOT_CONTROL_ERROR", "Unable to process control message");
            }
            self->received.consume(self->received.size());
            self->Read();
        });
    }

    void Receive(const boost::json::object& message) {
        const auto type = get_string(message, "type");
        std::lock_guard<std::mutex> transaction(owner.plan_requests_mutex_);
        if (type == "subscribe_pilot_control") {
            if (subscribed) return;
            subscribed = true;
            const auto aircraft = owner.PilotAircraftSnapshot();
            const auto connections = owner.PilotControlConnections();
            owner.pilot_control_->RefreshView(aircraft, connections);
            auto snapshot = owner.pilot_control_->Snapshot(actor, aircraft, connections, id);
            published_version = owner.pilot_control_->Version();
            Enqueue(Message::Subscribed, {{"type", "pilot_control_subscribed"},
                                         {"connection_id", id}, {"payload", std::move(snapshot)}});
            owner.PublishPilotControlState(&aircraft);
            return;
        }
        if (!subscribed) {
            Error("SUBSCRIPTION_REQUIRED", "Subscribe before sending control input");
            return;
        }
        if (type != "pilot_input") {
            Error("INVALID_REQUEST", "Unsupported control message type");
            return;
        }
        if (actor.role != PilotControlStore::Role::Pilot) {
            Error("ROLE_FORBIDDEN", "Only a Pilot may send control input");
            return;
        }
        if (!owner.pilot_control_->Input(actor, id, message, PilotControlStore::Clock::now())) {
            Enqueue(Message::Error, {{"type", "pilot_input_rejected"}, {"code", "INPUT_REJECTED"}});
        }
    }

    void Error(const std::string& code, const std::string& message) {
        Enqueue(Message::Error, {{"type", "error"}, {"code", code}, {"message", message}});
    }

    void Write() {
        if (finished) return;
        {
            std::lock_guard<std::mutex> lock(mailbox_mutex);
            auto next = std::find_if(pending.begin(), pending.end(), [](const auto& item) {
                return item.has_value();
            });
            if (next == pending.end()) {
                writing = false;
                return;
            }
            sending = std::move(**next);
            next->reset();
        }
        ws.text(true);
        ws.async_write(net::buffer(sending), [self = shared_from_this()](beast::error_code error, std::size_t) {
            if (error) self->Finish();
            else self->Write();
        });
    }

    void Finish() {
        if (finished) return;
        finished = true;
        {
            std::lock_guard<std::mutex> lock(mailbox_mutex);
            mailbox_closed = true;
        }
        beast::error_code ignored;
        ws.next_layer().shutdown(tcp::socket::shutdown_both, ignored);
        ws.next_layer().close(ignored);
        std::lock_guard<std::mutex> transaction(owner.plan_requests_mutex_);
        owner.pilot_connections_.erase(id);
        if (subscribed) {
            owner.pilot_control_->Disconnect(id);
            owner.PublishPilotControlState();
        }
    }

    tcp::socket::executor_type executor;
    ws_ns::stream<tcp::socket> ws;
    beast::flat_buffer received;
    std::string sending;
    bool finished = false;
    std::mutex mailbox_mutex;
    std::array<std::optional<std::string>, static_cast<std::size_t>(Message::Count)> pending;
    bool writing = false;
    bool mailbox_closed = false;
};

void HttpServer::LoadPilotControlCredentials(bool explicitMock) {
    if (!explicitMock) return;
#ifdef _WIN32
    wchar_t* environment_value = nullptr;
    std::size_t environment_length = 0;
    const auto read_error = _wdupenv_s(&environment_value, &environment_length, L"DRONE_PILOT_CONTROL_CREDENTIALS");
    const std::unique_ptr<wchar_t, decltype(&std::free)> credential_file(environment_value, &std::free);
    if (read_error != 0) throw std::runtime_error("Unable to read Pilot control environment setting");
    if (!credential_file || environment_length <= 1) return;
    const std::filesystem::path credential_path(credential_file.get());
#else
    const char* credential_file = std::getenv("DRONE_PILOT_CONTROL_CREDENTIALS");
    if (!credential_file || !*credential_file) return;
    const std::filesystem::path credential_path(credential_file);
#endif

    std::ifstream input(credential_path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open the Pilot control credential file");
    const std::string data((std::istreambuf_iterator<char>(input)), {});
    beast::error_code parse_error;
    const auto document = boost::json::parse(data, parse_error);
    if (parse_error || !document.is_object()) {
        throw std::runtime_error("Invalid Pilot control credential file");
    }
    const auto* entries = document.as_object().if_contains("credentials");
    if (!entries || !entries->is_array() || entries->as_array().empty()) {
        throw std::runtime_error("Pilot control credentials must be a nonempty array");
    }

    using Role = PilotControlStore::Role;
    std::set<std::pair<Role, std::string>> actors;
    for (const auto& value : entries->as_array()) {
        if (!value.is_object()) throw std::runtime_error("Invalid Pilot control credential entry");
        const auto& entry = value.as_object();
        for (const char* key : {"token", "role", "id"}) {
            const auto* field = entry.if_contains(key);
            if (!field || !field->is_string()) {
                throw std::runtime_error("Invalid Pilot control credential entry");
            }
        }
        const std::string token(entry.at("token").as_string());
        const std::string role(entry.at("role").as_string());
        const std::string id(entry.at("id").as_string());
        if (token.empty() || token.size() > 512 || id.empty() || id.size() > 128 ||
            token.find_first_of(" \t\r\n") != std::string::npos ||
            (role != "Command" && role != "Pilot")) {
            throw std::runtime_error("Invalid Pilot control credential entry");
        }

        PilotControlStore::Actor actor{role == "Command" ? Role::Command : Role::Pilot, id};
        if (!actors.emplace(actor.role, actor.id).second || !pilot_credentials_.emplace(token, actor).second) {
            throw std::runtime_error("Duplicate Pilot control credential or identity");
        }
    }
    pilot_control_ = std::make_unique<PilotControlStore>(
        std::filesystem::path(config_.storage_path).parent_path() / "pilot_control.json");
}

const PilotControlStore::Actor* HttpServer::PilotControlActor(
    const http::request<http::string_body>& request) const {
    const auto header = request[http::field::authorization];
    constexpr std::size_t prefix_length = 7;
    if (header.size() <= prefix_length || !beast::iequals(header.substr(0, prefix_length), "Bearer ")) {
        return nullptr;
    }
    const auto credential = pilot_credentials_.find(std::string(header.substr(prefix_length)));
    return credential == pilot_credentials_.end() ? nullptr : &credential->second;
}

PilotControlStore::Connections HttpServer::PilotControlConnections() const {
    PilotControlStore::Connections connections;
    for (const auto& [id, socket] : pilot_connections_) {
        if (socket->subscribed && socket->actor.role == PilotControlStore::Role::Pilot) {
            connections.emplace(id, socket->actor.id);
        }
    }
    return connections;
}

http::response<http::string_body> HttpServer::HandlePilotControlHttp(
    const http::request<http::string_body>& request) {
    const auto response = [&](int status, const boost::json::object& body) {
        auto reply = MakeResponse(request, status, json_stringify(body));
        reply.set(http::field::cache_control, "no-store");
        return reply;
    };
    const auto error = [&](int status, const std::string& code, const std::string& message) {
        return response(status, {{"code", code}, {"message", message}, {"params", boost::json::object{}}});
    };
    if (!pilot_control_) return error(404, "PILOT_CONTROL_DISABLED", "Pilot control is unavailable");
    const auto* actor = PilotControlActor(request);
    if (!actor) return error(401, "UNAUTHENTICATED", "Pilot control credentials are required");
    if (request.method() != http::verb::get && request.method() != http::verb::post) {
        return error(405, "METHOD_NOT_ALLOWED", "Use GET or POST for Pilot control");
    }

    try {
        boost::json::value body;
        if (request.method() == http::verb::post) body = parse_body(request);
        std::lock_guard<std::mutex> transaction(plan_requests_mutex_);
        const auto aircraft = PilotAircraftSnapshot();
        const auto connections = PilotControlConnections();
        pilot_control_->RefreshView(aircraft, connections);
        if (request.method() == http::verb::get) {
            return response(200, pilot_control_->Snapshot(*actor, aircraft, connections));
        }
        bool released = false;
        auto result = pilot_control_->Apply(*actor, require_object(body, "body"), aircraft, connections,
            PilotControlStore::Clock::now(), [this, &released](const PilotControlStore::Aircraft& plane) {
                try {
                    security_plans_.releaseMockPilotMission(plane.uav_id, plane.handoff_execution_id,
                        plane.handoff_control_version,
                        [this](const auto& event) { ws_manager_.broadcast(json_stringify(event)); });
                    released = true;
                } catch (const PlanError& failure) {
                    throw PilotControlError(409, failure.code, failure.what());
                }
            });
        PublishPilotControlState(released ? nullptr : &aircraft);
        return response(200, result);
    } catch (const PilotControlError& failure) {
        return error(failure.status_code, failure.code, failure.what());
    } catch (const ApiError& failure) {
        return error(failure.status_code, "INVALID_REQUEST", failure.what());
    } catch (const std::exception&) {
        spdlog::error("Pilot control request could not be processed");
        return error(500, "PILOT_CONTROL_ERROR", "Unable to process Pilot control request");
    }
}

void HttpServer::RunPilotControlWsSession(tcp::socket socket,
                                         http::request<http::string_body> request) {
    const auto reject = [&](int status, const char* code) {
        const auto reply = MakeResponse(request, status, json_stringify(boost::json::object{{"code", code}}));
        http::write(socket, reply);
    };
    const auto* actor = PilotControlActor(request);
    if (!pilot_control_ || !actor) {
        if (pilot_control_) reject(401, "UNAUTHENTICATED");
        else reject(404, "PILOT_CONTROL_DISABLED");
        return;
    }
    if (const auto marker = request.find("X-Drone-Pilot-Sim-Bridge"); marker != request.end()) {
        if (marker->value() != "1") {
            reject(400, "INVALID_PILOT_SIM_BRIDGE_HEADER");
            return;
        }
        if (!pilot_sim_bridge_enabled_) {
            reject(404, "PILOT_SIM_BRIDGE_DISABLED");
            return;
        }
        beast::error_code error;
        const auto peer = socket.remote_endpoint(error);
        if (error || !peer.address().is_loopback()) {
            reject(403, "PILOT_SIM_BRIDGE_LOOPBACK_REQUIRED");
            return;
        }
        if (actor->role != PilotControlStore::Role::Pilot) {
            reject(403, "ROLE_FORBIDDEN");
            return;
        }
    }

    static const auto process_id = std::chrono::system_clock::now().time_since_epoch().count();
    static std::atomic<uint64_t> sequence{0};
    const auto connection_id = "PC-" + std::to_string(process_id) + "-" + std::to_string(++sequence);
    auto session = std::make_shared<PilotSocket>(*this, std::move(socket), *actor, connection_id);
    {
        std::lock_guard<std::mutex> transaction(plan_requests_mutex_);
        if (!running_) return;
        pilot_connections_.emplace(connection_id, session);
    }
    session->Run(request);
}

void HttpServer::PublishPilotControlState(const std::vector<PilotControlStore::Aircraft>* sampled) {
    if (!pilot_control_ || pilot_connections_.empty()) return;
    std::vector<PilotControlStore::Aircraft> current;
    if (!sampled) {
        current = PilotAircraftSnapshot();
        sampled = &current;
    }
    const auto connections = PilotControlConnections();
    pilot_control_->RefreshView(*sampled, connections);
    const auto version = pilot_control_->Version();
    for (const auto& [id, socket] : pilot_connections_) {
        if (!socket->subscribed || socket->published_version == version) continue;
        socket->Enqueue(PilotSocket::Message::State,
                        {{"type", "pilot_control_changed"},
                         {"payload", pilot_control_->Snapshot(socket->actor, *sampled, connections, id)}});
        socket->published_version = version;
    }
    TickPilotControlSockets();
}

void HttpServer::TickPilotControlSockets() {
    if (!pilot_control_) return;
    const auto now = PilotControlStore::Clock::now();
    for (const auto& [id, socket] : pilot_connections_) {
        if (!socket->subscribed || socket->actor.role != PilotControlStore::Role::Pilot) continue;
        auto window = pilot_control_->InputWindow(socket->actor, id, now);
        if (!window.empty() && window != socket->last_window) {
            socket->Enqueue(PilotSocket::Message::Window, window);
        } else if (window.empty() && !socket->last_window.empty()) {
            socket->ClearWindow();
        }
        socket->last_window = std::move(window);
    }
}

void HttpServer::StopPilotControlSockets() {
    std::lock_guard<std::mutex> transaction(plan_requests_mutex_);
    for (const auto& entry : pilot_connections_) entry.second->Close();
}
