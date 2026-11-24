    static bool mockSimulationTool(const std::string& tool) {
        return tool == "create_mock_simulated_devices" || tool == "update_mock_device_metadata" ||
               tool == "remove_mock_test_device" || tool == "start_mock_rehearsal" ||
               tool == "record_mock_rehearsal_review";
    }

    static int64_t mockDeviceVersion(const Object& device) {
        const auto* version = device.if_contains("version");
        return version ? version->to_number<int64_t>() : 1;
    }

    Object mockFleetWrite(Object& next, const MockPlanCall& call, const std::set<std::string>& registered) {
        auto& devices = next.at("simulation_uavs").as_object();
        const auto& args = call.arguments;
        const auto version = next.at("version").to_number<int64_t>() + 1;
        if (call.tool == "create_mock_simulated_devices") {
            const auto* origin = args.if_contains("resolved_device_origin");
            if (!origin || !origin->is_object()) throw PlanError("FIXTURE_NOT_FOUND", "Choose an approved device fixture");
            auto parameters = origin->as_object();
            parameters["count"] = args.at("count");
            const auto before = devices;
            createSimulationAircraft(next, parameters, registered, 20);
            Array created;
            for (auto& entry : devices) {
                if (before.contains(entry.key())) continue;
                auto& device = entry.value().as_object();
                device["version"] = version;
                device["updated_at"] = OperationalContext::now();
                created.emplace_back(Object{{"device_id", entry.key()}, {"name", device.at("name")},
                                            {"kind", "simulated"}, {"version", version}});
            }
            const auto count = created.size();
            return {{"devices", std::move(created)}, {"created_count", count}};
        }
        const auto id = str(args, "device_id");
        auto& device = lookup(devices, id, "NOT_FOUND");
        if (args.at("expected_version") != boost::json::value(mockDeviceVersion(device)))
            throw PlanError("VERSION_CONFLICT", "Read the current simulator registry version");
        if (call.tool == "update_mock_device_metadata") {
            if (!args.contains("name") && !args.contains("label"))
                throw PlanError("INVALID_ARGUMENT", "Supply a simulator name or label");
            for (const auto* field : {"name", "label"}) if (args.contains(field)) device[field] = args.at(field);
            device["version"] = version;
            device["updated_at"] = OperationalContext::now();
            return {{"device_id", id}, {"name", device.at("name")}, {"label", str(device, "label")}, {"version", version}};
        }
        checkMockPlanApproval(call);
        for (const auto& item : next.at("missions").as_object()) {
            const auto members = assignment(item.value().as_object());
            if (std::find(members.begin(), members.end(), boost::json::value(id)) != members.end())
                throw PlanError("OBJECT_IN_USE", "Clear simulator assignment before removing the device");
        }
        for (const auto& item : next.at("executions").as_object()) {
            const auto& run = item.value().as_object();
            if (str(run, "uav_id") == id && executionActive(run))
                throw PlanError("OBJECT_IN_USE", "The simulator has an active or scheduled run");
        }
        devices.erase(id);
        return {{"device_id", id}, {"removed", true}};
    }

    Object mockRehearsalWrite(Object& next, const MockPlanCall& call) {
        const auto& args = call.arguments;
        auto& executions = next.at("executions").as_object();
        if (call.tool == "start_mock_rehearsal") {
            const auto planId = str(args, "plan_id");
            const auto& plan = lookup(next.at("plans").as_object(), planId, "PLAN_NOT_FOUND");
            if (plan.contains("real_assignment")) throw PlanError("PLAN_NOT_FOUND", "Unknown Mock plan");
            const auto& revision = plan.at("content_revision");
            if (args.at("expected_content_revision") != revision)
                throw PlanError("VERSION_CONFLICT", "Read the current rehearsal input revision");
            const Object request{{"action", "simulation_start"}, {"instance_id", call.owner()},
                                 {"plan_id", planId}, {"mission_id", args.at("mission_id")}};
            checkConfigurationRequest(next, request);
            const auto firstId = startRehearsal(next, request);
            const auto group = str(executions.at(firstId).as_object(), "group_id");
            for (auto& item : executions) {
                auto& row = item.value().as_object();
                if (str(row, "group_id") == group) row["input_content_revision"] = revision;
            }
            next["execution_version"] = next.at("execution_version").to_number<int64_t>() + 1;
            return {{"rehearsal_id", group}, {"plan_id", planId}, {"mission_id", args.at("mission_id")},
                    {"state", "created"}, {"version", 0}, {"input_content_revision", revision}, {"review_state", "pending"}};
        }
        const auto group = str(args, "rehearsal_id");
        std::string firstId;
        int64_t version = 0;
        for (const auto& item : executions) {
            const auto& row = item.value().as_object();
            if (!isRehearsal(row) || str(row, "group_id") != group || row.at("simulation") != true) continue;
            if (firstId.empty()) firstId = std::string(item.key());
            version += row.at("control_version").to_number<int64_t>();
        }
        if (firstId.empty()) throw PlanError("NOT_FOUND", "Choose a software rehearsal in this workspace");
        if (args.at("expected_version") != boost::json::value(version))
            throw PlanError("VERSION_CONFLICT", "Read the current rehearsal version");
        checkMockPlanApproval(call);
        auto& first = executions.at(firstId).as_object();
        const auto planId = str(first, "plan_id");
        const auto& plan = lookup(next.at("plans").as_object(), planId, "PLAN_NOT_FOUND");
        const auto* revision = first.if_contains("input_content_revision");
        if (revision && *revision != plan.at("content_revision"))
            throw PlanError("REHEARSAL_STALE", "Run a new rehearsal for the current plan revision");
        const Object request{{"action", "simulation_review"}, {"instance_id", call.owner()}, {"plan_id", planId}};
        checkConfigurationRequest(next, request);
        if (first.contains("rehearsal_review")) throw PlanError("INVALID_STATE", "This rehearsal is already reviewed");
        reviewRehearsal(next, first, "Mock MCP");
        first["control_version"] = first.at("control_version").to_number<int64_t>() + 1;
        first["updated_at"] = OperationalContext::now();
        return {{"rehearsal_id", group}, {"state", "completed"}, {"version", version + 1}, {"review_state", "approved"},
                {"reviewed_at", first.at("rehearsal_review").as_object().at("reviewed_at")}};
    }

    Object commitMockSimulation(Object& next, const MockPlanCall& call, const std::set<std::string>& registered,
                                const std::function<void(const Object&)>& publish) {
        const bool rehearsal = call.tool == "start_mock_rehearsal" || call.tool == "record_mock_rehearsal_review";
        auto result = rehearsal ? mockRehearsalWrite(next, call) : mockFleetWrite(next, call, registered);
        result["request_id"] = call.arguments.at("request_id");
        result["confirmed_at"] = OperationalContext::now();
        next["version"] = state_.at("version").to_number<int64_t>() + 1;
        const bool approval = call.tool == "remove_mock_test_device" || call.tool == "record_mock_rehearsal_review";
        result = persistMockMcpResult(next, call, std::move(result), approval, publish);
        if (!rehearsal) publish({{"type", "DroneRegistryChanged"}});
        return result;
    }
