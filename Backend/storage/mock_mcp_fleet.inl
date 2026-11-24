    static bool mockFleetTool(const std::string& tool) {
        return tool == "preview_mock_fleet_move" || tool == "preview_mock_single_takeover" ||
               tool == "confirm_mock_fleet_move" || tool == "confirm_mock_single_takeover" ||
               tool == "cancel_mock_fleet_preview";
    }

    static bool mockMotionExecution(const Object& row) {
        return row.at("simulation") == true && str(row, "adapter") == "Mock" && !isRehearsal(row);
    }

    Object mockFleetGuard(const Object& next, const Object& request) const {
        const auto source = str(request, "execution_id");
        if (!source.empty()) {
            const auto& rows = next.at("executions").as_object();
            const auto& execution = lookup(rows, source, "EXECUTION_NOT_FOUND");
            if (!mockMotionExecution(execution)) throw PlanError("EXECUTION_NOT_FOUND", "Choose a Mock execution");
            if (str(execution, "uav_id") != str(request, "uav_id"))
                throw PlanError("EXECUTION_STATE_INVALID", "Select the device belonging to this execution");
            if (number(execution, "control_version") != number(request, "control_version"))
                throw PlanError("EXECUTION_STALE", "Read the current execution version");
            const auto group = str(execution, "group_id");
            Array others;
            for (const auto& entry : rows) {
                const auto& row = entry.value().as_object();
                if (!group.empty() && str(row, "group_id") == group && entry.key() != source) {
                    if (!mockMotionExecution(row)) throw PlanError("EXECUTION_NOT_FOUND", "Choose a Mock execution group");
                    others.push_back(row.at("uav_id"));
                }
            }
            return {{"source_execution_id", source}, {"affected_device_id", execution.at("uav_id")},
                    {"unaffected_device_ids", std::move(others)}};
        }

        Object guard;
        for (const auto& member : request.at("assigned_uav_ids").as_array()) {
            const std::string device(member.as_string());
            if (!mock_.at("uavs").as_object().contains(device))
                throw PlanError("MOCK_UAV_UNAVAILABLE", "Choose a provisioned simulator fixture");
            guard[device] = currentPosition(next, device);
        }
        return guard;
    }

    Object writeMockFleet(Object& next, const MockPlanCall& call, const std::function<void(const Object&)>& publish,
                          const std::function<void(const std::string&)>& checkPilotOwner) {
        const auto now = OperationalContext::now();
        const auto& args = call.arguments;
        const bool create = call.tool == "preview_mock_fleet_move" || call.tool == "preview_mock_single_takeover";
        const bool cancel = call.tool == "cancel_mock_fleet_preview";
        const bool takeover = call.tool == "preview_mock_single_takeover" || call.tool == "confirm_mock_single_takeover";
        const std::string kind = takeover ? "single_takeover" : "fleet_move";
        if (!next.contains("mock_mcp_fleet_previews")) next["mock_mcp_fleet_previews"] = Object{};
        auto& previews = next.at("mock_mcp_fleet_previews").as_object();
        auto id = str(args, "preview_id");
        Object result{{"request_id", args.at("request_id")}, {"confirmed_at", now}};
        std::set<std::string> fixtures;
        for (const auto& row : mock_.at("uavs").as_object()) fixtures.emplace(row.key());
        if (create) {
            const auto& fixture = args.at("resolved_motion_fixture");
            if (!fixture.is_object()) throw PlanError("FIXTURE_NOT_FOUND", "Choose an approved simulator motion fixture");
            if (activeConfiguration(next)) configurationConflict(*activeConfiguration(next));
            id = allocate(next, "fleet-preview-");
            Object request{{"instance_id", call.owner()}, {"request_id", id},
                           {"action", takeover ? "execution_takeover_move" : "execution_group_move"},
                           {"target", fixture.as_object().at("point")}};
            if (const auto* radius = fixture.as_object().if_contains("radius")) request["target_area_radius_m"] = *radius;
            if (takeover) {
                request["execution_id"] = args.at("execution_id");
                request["uav_id"] = args.at("device_id");
                request["control_version"] = args.at("expected_execution_version");
                if (!fixtures.count(str(request, "uav_id"))) throw PlanError("MOCK_UAV_UNAVAILABLE", "Unknown Mock device");
            } else {
                request["assigned_uav_ids"] = args.at("device_ids");
                result["device_ids"] = args.at("device_ids");
            }
            const auto guard = mockFleetGuard(next, request);
            if (takeover) for (const auto& entry : guard) result[entry.key()] = entry.value();
            Array checks;
            auto dryRun = next;
            try {
                executionRequest(request, fixtures, "Mock MCP", publish, checkPilotOwner, nullptr, &dryRun);
            } catch (const PlanError& error) {
                if (takeover && error.code == "EXECUTION_STATE_INVALID") throw;
                checks.emplace_back(Object{{"code", error.code}, {"message", error.code}, {"severity", "blocking"}});
            }
            result["preview_id"] = id;
            result["kind"] = kind;
            result["state"] = checks.empty() ? "ready" : "blocked";
            result["expires_at"] = now + 60;
            result["checks"] = std::move(checks);
            previews[id] = Object{{"owner", call.owner()}, {"guard", guard}, {"request", request}, {"result", result}};
        } else {
            auto& preview = lookup(previews, id, "NOT_FOUND");
            if (str(preview, "owner") != call.owner()) throw PlanError("NOT_FOUND", "Choose your own preview");
            auto staged = preview.at("result").as_object();
            if (!cancel && str(staged, "kind") != kind) throw PlanError("INVALID_STATE", "Use the matching confirmation tool");
            if (preview.contains("closed")) throw PlanError("INVALID_STATE", "The preview is closed");
            if (number(staged, "expires_at") <= now) throw PlanError("PREVIEW_EXPIRED", "Create a fresh preview");
            if (cancel) {
                preview["closed"] = true;
                result["preview_id"] = id;
                result["kind"] = staged.at("kind");
                result["state"] = "canceled";
            } else {
                if (str(staged, "state") != "ready") throw PlanError("PREVIEW_BLOCKED", "Resolve preview checks first");
                checkMockPlanApproval(call);
                const auto request = preview.at("request").as_object();
                try {
                    if (mockFleetGuard(next, request) != preview.at("guard").as_object())
                        throw PlanError("PREVIEW_STALE", "Simulator state changed");
                } catch (const PlanError&) {
                    throw PlanError("PREVIEW_STALE", "Simulator state changed");
                }
                preview["closed"] = true;
                try {
                    return executionRequest(request, fixtures, "Mock MCP", publish, checkPilotOwner, &call, &next);
                } catch (const PlanError&) {
                    throw PlanError("PREVIEW_STALE", "Simulator state changed");
                }
            }
        }
        next["version"] = state_.at("version").to_number<int64_t>() + 1;
        return persistMockMcpResult(next, call, std::move(result), false, publish);
    }
