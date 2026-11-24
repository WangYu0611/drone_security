boost::json::object HttpServer::MockMcpWriteView(
    const std::string& identity, const std::string& tool, const boost::json::object& arguments) {
    using Object = boost::json::object;
    using Value = boost::json::value;
    const auto request = arguments.at("request_id");
    const auto session = arguments.at("session_id");
    const auto publish = [this](const Object& event) { ws_manager_.broadcast(json_stringify(event)); };
    const auto now = OperationalContext::now();
    const std::string owner = "mock-mcp-" + identity;
    MockViewReceipts::Write write{
        identity + "/" + tool + "/" + std::string(request.as_string()), arguments, {}, {}};
    write.arguments.erase("resolved_source_id");
    write.arguments.erase("source_device_mismatch");

    if (tool == "set_mock_workspace_language") {
        write.result = [&](const Object& state) {
            return Object{{"request_id", request}, {"session_id", session}, {"confirmed_at", now},
                {"version", state.at("ui_preferences_version")}, {"language", state.at("language")}};
        };
        return ui_preferences_.update({{"language", arguments.at("language")}}, {}, owner, publish, &write);
    }

    std::set<std::string> devices;
    for (const auto& entry : security_plans_.simulationFleet()) devices.emplace(entry.key());
    for (const auto& entry : security_plans_.mockTelemetryPositions()) devices.emplace(entry.key());
    if (tool == "set_mock_video_target") {
        const auto device = arguments.at("device_id");
        const auto source = arguments.at("resolved_source_id");
        write.validate = [&] {
            if (!device.is_null() && !devices.count(std::string(device.as_string())))
                throw MockViewError("NOT_FOUND");
            if (arguments.contains("source_id") &&
                (device.is_null() || arguments.at("source_device_mismatch") == true))
                throw MockViewError("INVALID_ARGUMENT");
            if (arguments.contains("source_id") && source.is_null()) throw MockViewError("NOT_FOUND");
        };
        write.result = [&](const Object& state) {
            const auto* stored = state.if_contains("mock_source_id");
            return Object{{"request_id", request}, {"session_id", session}, {"confirmed_at", now},
                {"version", state.at("video_view_version")}, {"target_device_id", state.at("video_target_uav_id")},
                {"source_id", stored ? *stored : Value(nullptr)},
                {"playback_state", !device.is_null() && !stored ? "no_source" : "unreported"},
                {"playback_observed_at", nullptr}};
        };
        return video_view_.update({{"video_target_uav_id", device}, {"mock_source_id", source}},
            devices, owner, publish, &write);
    }

    const auto video_target = video_view_.snapshot().at("video_target_uav_id");
    write.validate = [&] {
        try {
            const auto plan = SecurityPlanStore::str(arguments, "plan_id");
            const auto mission = SecurityPlanStore::str(arguments, "mission_id");
            security_plans_.checkSelection(plan, mission);
            const auto snapshot = security_plans_.snapshot();
            if (!plan.empty() && snapshot.at("plans").as_object().at(plan).as_object().contains("real_assignment"))
                throw MockViewError("INVALID_ARGUMENT");
        } catch (const PlanError&) {
            throw MockViewError("INVALID_ARGUMENT");
        }
        for (const auto& device : arguments.at("device_ids").as_array()) {
            if (!devices.count(std::string(device.as_string()))) throw MockViewError("INVALID_ARGUMENT");
        }
    };
    write.result = [&](const Object& state) {
        const bool known_target = video_target.is_string() && devices.count(std::string(video_target.as_string()));
        return Object{{"request_id", request}, {"session_id", session}, {"confirmed_at", now},
            {"version", state.at("context_version")}, {"plan_id", state.at("active_security_plan_id")},
            {"mission_id", state.at("active_mission_id")}, {"device_ids", state.at("selected_uav_ids")},
            {"primary_device_id", state.at("active_uav_id")},
            {"video_target_device_id", known_target ? video_target : Value(nullptr)}};
    };
    return operational_context_.update({
        {"active_security_plan_id", arguments.at("plan_id")}, {"active_mission_id", arguments.at("mission_id")},
        {"selected_uav_ids", arguments.at("device_ids")}, {"active_uav_id", arguments.at("primary_device_id")},
    }, owner, publish, &write);
}
