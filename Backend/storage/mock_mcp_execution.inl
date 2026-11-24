    static bool mockExecutionTool(const std::string& tool) {
        return tool == "request_mock_execution" || tool == "update_mock_execution_schedule" ||
               tool == "start_scheduled_mock_execution" || tool == "cancel_mock_execution_schedule" ||
               tool == "set_mock_execution_state";
    }

    static bool mockExecutionApprovalRequired(const MockPlanCall& call) {
        return call.tool == "request_mock_execution" || call.tool == "start_scheduled_mock_execution" ||
               (call.tool == "set_mock_execution_state" && str(call.arguments, "action") == "abort");
    }

    Object writeMockExecution(const Object& next, const MockPlanCall& call,
                              const std::function<void(const Object&)>& publish,
                              const std::function<void(const std::string&)>& checkPilotOwner) {
        const auto& args = call.arguments;
        const bool create = call.tool == "request_mock_execution";
        Object request{{"instance_id", call.owner()}, {"request_id", call.scope() + "/" + str(args, "request_id")}};
        if (create) {
            const auto deployment = str(args, "deployment_id");
            const auto& saved = lookup(next.at("deployments").as_object(), deployment, "DEPLOYMENT_MISSING");
            const auto planId = str(saved, "plan_id");
            const auto& plan = lookup(next.at("plans").as_object(), planId, "PLAN_NOT_FOUND");
            if (plan.contains("real_assignment") || str(plan, "deployment_id") != deployment)
                throw PlanError("INVALID_STATE", "Select the current Mock deployment");
            if (!saved.at("snapshot").as_object().at("missions").as_object().contains(str(args, "mission_id")))
                throw PlanError("INVALID_STATE", "Select a mission from this deployment");
            if ((str(args, "mode") == "scheduled") != args.contains("scheduled_for"))
                throw PlanError("INVALID_ARGUMENT", "Supply a timestamp only for scheduled execution");
            request["action"] = "execution_start";
            request["plan_id"] = planId;
            request["mission_id"] = args.at("mission_id");
        } else {
            const auto id = str(args, "execution_id");
            const auto& rows = next.at("executions").as_object();
            const auto& execution = lookup(rows, id, "EXECUTION_NOT_FOUND");
            const auto group = str(execution, "group_id");
            for (const auto& entry : rows) {
                const auto& member = entry.value().as_object();
                if ((entry.key() == id || (!group.empty() && str(member, "group_id") == group)) &&
                    (member.at("simulation") != true || str(member, "adapter") != "Mock" ||
                     (member.if_contains("rehearsal") && member.at("rehearsal") == true)))
                    throw PlanError("EXECUTION_NOT_FOUND", "Choose an isolated Mock execution");
            }
            if (call.tool == "update_mock_execution_schedule") request["action"] = "execution_reschedule";
            else if (call.tool == "start_scheduled_mock_execution") request["action"] = "execution_start_now";
            else if (call.tool == "cancel_mock_execution_schedule") request["action"] = "execution_cancel";
            else {
                const auto action = str(args, "action");
                if (action != "pause" && action != "resume" && action != "return" && action != "abort")
                    throw PlanError("INVALID_ARGUMENT", "Unsupported Mock execution action");
                if (action != "abort" && args.contains("approval_id"))
                    throw PlanError("INVALID_ARGUMENT", "Supply approval only for abort");
                request["action"] = "execution_" + action;
            }
            request["execution_id"] = id;
            request["control_version"] = args.at("expected_version");
            if (str(execution, "state") == "SCHEDULED" && execution.contains("mock_mcp_start_requested"))
                throw PlanError("EXECUTION_STATE_INVALID", "This execution has already been queued to start");
        }
        if (args.contains("scheduled_for")) request["scheduled_start_at"] = args.at("scheduled_for");
        if (mockExecutionApprovalRequired(call)) checkMockPlanApproval(call);
        std::set<std::string> fixtures;
        for (const auto& row : mock_.at("uavs").as_object()) fixtures.emplace(row.key());
        return executionRequest(request, fixtures, "Mock MCP", publish, checkPilotOwner, &call);
    }

    Object commitMockExecution(Object& next, const MockPlanCall& call, const std::string& id,
                               const std::function<void(const Object&)>& publish) {
        const auto& row = next.at("executions").as_object().at(id).as_object();
        auto state = str(row, "state");
        std::transform(state.begin(), state.end(), state.begin(), [](unsigned char c) { return std::tolower(c); });
        if (call.tool == "start_scheduled_mock_execution") state = "starting";
        Object result{{"request_id", call.arguments.at("request_id")}, {"execution_id", id},
            {"state", state == "cancelled" ? "canceled" : state}, {"version", row.at("control_version")},
            {"confirmed_at", OperationalContext::now()}};
        if (call.tool == "request_mock_execution") {
            result["deployment_id"] = row.at("deployment_id");
            result["mission_id"] = row.at("mission_id");
        }
        if (call.tool == "request_mock_execution" || call.tool == "update_mock_execution_schedule" ||
            call.tool == "start_scheduled_mock_execution") {
            const auto* scheduled = row.if_contains("scheduled_start_epoch");
            result["scheduled_for"] = state == "scheduled" && scheduled ? *scheduled : boost::json::value(nullptr);
        }
        next["version"] = state_.at("version").to_number<int64_t>() + 1;
        return persistMockMcpResult(next, call, std::move(result), mockExecutionApprovalRequired(call), publish);
    }
