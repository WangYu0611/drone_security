    // Mock MCP calls share the normal plan transaction and commit their receipt
    // with the same durable document. The namespaced ledger is hidden from UI snapshots.
    struct MockPlanCall {
        std::string identity;
        std::string tool;
        Object arguments;

        std::string owner() const { return "mock-mcp-" + identity; }
        std::string scope() const { return "plan-write/" + identity + "/" + tool; }
    };

    static std::string mockConfigurationId(const Object& lease) {
        return "configuration-" + std::to_string(lease.at("acquired_version").to_number<int64_t>());
    }

    Object writeMockMcpPlan(std::string identity, std::string tool, Object arguments, const std::set<std::string>& registered,
                            const std::function<void(const Object&)>& publish) {
        const MockPlanCall call{std::move(identity), std::move(tool), std::move(arguments)};
        return transact({}, registered, "Mock MCP", publish, {}, &call);
    }

private:
    #include "storage/mock_mcp_route.inl"

    static Object mockPlanFingerprint(const MockPlanCall& call) {
        auto arguments = call.arguments;
        arguments.erase("resolved_fixture_path");
        arguments.erase("resolved_device_origin");
        arguments.erase("approval_id");
        return arguments;
    }

    #include "storage/mock_mcp_approval.inl"
    #include "storage/mock_mcp_simulation.inl"

    const Object* mockMcpReceipt(const MockPlanCall& call) const {
        const auto* scope = mock_mcp_draft_receipts_.if_contains(call.scope());
        const auto* value = scope ? scope->as_object().if_contains(str(call.arguments, "request_id")) : nullptr;
        if (!value) return nullptr;
        const auto& receipt = value->as_object();
        if (receipt.at("arguments") != mockPlanFingerprint(call))
            throw PlanError("REQUEST_ID_CONFLICT", "Use the original parameters for this request_id");
        return &receipt.at("result").as_object();
    }

    static void checkMockPlanVersion(const Object& arguments, const Object& plan, const char* field) {
        const auto* version = arguments.if_contains(field);
        if (!version || (!version->is_int64() && !version->is_uint64()) || *version != plan.at("version"))
            throw PlanError("VERSION_CONFLICT", "Read the current plan version");
    }

    Object mockPlanRequest(Object& next, const MockPlanCall& call) const {
        static const std::map<std::string, std::string> actions{
            {"begin_mock_plan_edit", "begin_configuration"}, {"end_mock_plan_edit", "end_configuration"},
            {"update_mock_security_plan_draft", "update_plan"}, {"copy_mock_security_plan_draft", "copy_plan"},
            {"reopen_mock_security_plan", "reopen_plan"}, {"add_mock_plan_mission", "add_mission"},
            {"rename_mock_plan_mission", "rename_mission"}, {"set_mock_mission_assignment", "assign"},
            {"validate_mock_plan", "validate"},
            {"delete_mock_security_plan_draft", "delete_plan"}, {"delete_mock_plan_mission", "delete_mission"},
            {"record_mock_plan_review", "review"}, {"publish_mock_plan_snapshot", "deploy"},
            {"begin_mock_route_edit", "begin_route_edit"}, {"save_mock_route_draft", "save_route"},
            {"end_mock_route_edit", "finish_route_edit"},
        };
        const auto found = actions.find(call.tool);
        if (found == actions.end()) throw PlanError("INVALID_ARGUMENT", "Unknown Mock plan operation");
        const auto& args = call.arguments;
        const auto action = call.tool == "end_mock_route_edit" && str(args, "disposition") == "discard_unsaved"
            ? "discard_route_edit" : found->second;
        if (mockRouteAction(action)) return mockRouteRequest(next, call, action);
        const auto* lease = activeConfiguration(next);
        auto planId = str(args, action == "copy_plan" ? "source_plan_id" : "plan_id");
        const auto missionId = str(args, "mission_id");
        const bool requiresHandle = action != "begin_configuration" && action != "copy_plan" && action != "reopen_plan" &&
                                    action != "delete_plan";
        if (requiresHandle) {
            if (!lease || str(*lease, "owner_instance_id") != call.owner() ||
                mockConfigurationId(*lease) != str(args, "configuration_id"))
                throw PlanError("CONFIGURATION_EXPIRED", "Acquire or read your current configuration session");
            if (action == "end_configuration") planId = str(*lease, "plan_id");
            if (planId != str(*lease, "plan_id"))
                throw PlanError("CONFIGURATION_EXPIRED", "The active configuration belongs to another plan");
        }
        const auto& plan = lookup(next.at("plans").as_object(), planId, "PLAN_NOT_FOUND");
        if (plan.contains("real_assignment")) throw PlanError("PLAN_NOT_FOUND", "Unknown Mock plan");
        if (action != "end_configuration")
            checkMockPlanVersion(args, plan, action == "copy_plan" ? "expected_source_plan_version" : "expected_plan_version");
        if (mockPlanApprovalRequired(action)) checkMockPlanApproval(call);

        Object request{{"action", action}, {"plan_id", planId}, {"mission_id", missionId},
                       {"instance_id", call.owner()}, {"expected_version", next.at("version")}};
        if (action == "copy_plan") {
            if (lease || !next.at("edit_sessions").as_object().empty())
                throw PlanError("CONFIGURATION_IN_PROGRESS", "Finish the active Mock configuration first");
            request["allow_draft"] = true;
            request["name"] = args.contains("name") ? args.at("name") : boost::json::value("Copy of " + planId);
        } else if (action == "reopen_plan") {
            if (lease) configurationConflict(*lease);
            if (str(plan, "status") != "DEPLOYED") throw PlanError("INVALID_STATE", "Only deployed plans can reopen");
            request["expected_content_revision"] = plan.at("content_revision");
            const auto now = OperationalContext::now();
            next["configuration_session"] = Object{{"owner_instance_id", call.owner()}, {"owner_client_id", "Mock MCP"},
                {"plan_id", planId}, {"last_seen_at", now}, {"lease_expires_at", now + 1},
                {"acquired_version", next.at("version").to_number<int64_t>() + 1}};
        } else if (action == "begin_configuration" && str(plan, "status") == "DEPLOYED") {
            throw PlanError("INVALID_STATE", "Reopen the deployed plan before editing");
        } else if (action == "update_plan") {
            if (!args.contains("name") && !args.contains("description"))
                throw PlanError("INVALID_ARGUMENT", "Supply a name or description");
            request["name"] = args.contains("name") ? args.at("name") : plan.at("name");
            request["description"] = args.contains("description") ? args.at("description") : plan.at("description");
        } else if (action == "add_mission" || action == "rename_mission") {
            request["name"] = args.at("name");
        } else if (action == "assign") {
            const auto& devices = args.at("device_ids").as_array();
            for (const auto& device : devices) {
                if (!device.is_string() || !next.at("simulation_uavs").as_object().contains(device.as_string()))
                    throw PlanError("NOT_FOUND", "Choose a simulated device from this workspace");
            }
            request["assigned_uav_ids"] = devices;
        } else if (action == "delete_plan" || action == "delete_mission") {
            if (str(plan, "status") != "DRAFT") throw PlanError("INVALID_STATE", "Only editable drafts can be removed");
            checkPlanNotExecuting(next, planId);
            if (action == "delete_plan") {
                if (lease) configurationConflict(*lease);
                request["confirmation_name"] = plan.at("name");
                request["expected_content_revision"] = plan.at("content_revision");
            } else {
                for (const auto& entry : next.at("edit_sessions").as_object()) {
                    if (str(entry.value().as_object(), "mission_id") == missionId)
                        throw PlanError("PLAN_EDIT_IN_PROGRESS", "Finish editing before removing the task");
                }
            }
        } else if (action == "review" || action == "deploy") {
            const auto* stage = plan.if_contains(action == "review" ? "validation" : "review");
            const auto key = action == "review" ? "validation_id" : "review_id";
            if (!stage || !stage->is_object() || str(stage->as_object(), key) != str(args, key))
                throw PlanError(action == "review" ? "CHECK_FAILED" : "REVIEW_REQUIRED", "Read the confirmed stage before continuing");
        }
        return request;
    }

    static void renewMockConfiguration(Object& next, const MockPlanCall& call) {
        if (const auto* lease = activeConfiguration(next); lease && str(*lease, "owner_instance_id") == call.owner()) {
            const auto now = OperationalContext::now();
            auto& current = next.at("configuration_session").as_object();
            current["last_seen_at"] = now;
            current["lease_expires_at"] = now + 120;
            for (auto& entry : next.at("edit_sessions").as_object()) {
                auto& edit = entry.value().as_object();
                if (str(edit, "owner_instance_id") == call.owner()) {
                    edit["last_seen_at"] = now;
                    edit["lease_expires_at"] = now + 120;
                }
            }
        }
    }

    static Object mockPlanResult(const Object& next, const MockPlanCall& call, const Object& request,
                                 const std::string& planId, const std::string& missionId) {
        const auto action = str(request, "action");
        if (mockRouteAction(action)) return mockRouteResult(next, call, request, planId, missionId);
        Object result{{"request_id", call.arguments.at("request_id")}, {"plan_id", planId},
                      {"confirmed_at", OperationalContext::now()}};
        if (action == "delete_plan") {
            result["plan_id"] = call.arguments.at("plan_id");
            result["deleted"] = true;
            return result;
        }
        if (action == "end_configuration") {
            result["configuration_id"] = call.arguments.at("configuration_id");
            result["released"] = true;
            return result;
        }
        const auto& plan = next.at("plans").as_object().at(planId).as_object();
        result["plan_version"] = plan.at("version");
        result["content_revision"] = plan.at("content_revision");
        if (action == "begin_configuration") {
            const auto& lease = next.at("configuration_session").as_object();
            result["configuration_id"] = mockConfigurationId(lease);
            result["lease_expires_at"] = lease.at("lease_expires_at");
        } else if (action == "copy_plan" || action == "update_plan" || action == "reopen_plan") {
            result["state"] = "draft";
            if (action != "reopen_plan") result["name"] = plan.at("name");
            if (action == "copy_plan") {
                result["source_plan_id"] = call.arguments.at("source_plan_id");
                result["mission_count"] = plan.at("mission_ids").as_array().size();
            } else if (action == "update_plan") result["description"] = plan.at("description");
            else result["previous_deployment_id"] = plan.at("previous_deployment_id");
        } else if (action == "validate") {
            const auto& validation = plan.at("validation").as_object();
            const bool passed = validation.at("ready").as_bool();
            result["state"] = passed ? "ready" : "draft";
            result["validation_id"] = validation.at("validation_id");
            result["passed"] = passed;
            Array checks;
            for (const auto& value : validation.at("issues").as_array()) {
                const auto code = str(value.as_object(), "code");
                checks.emplace_back(Object{{"code", code}, {"message", code}, {"severity", "blocking"}});
            }
            result["checks"] = std::move(checks);
        } else if (action == "review") {
            result["review_id"] = plan.at("review").as_object().at("review_id");
            result["validation_id"] = call.arguments.at("validation_id");
            result["state"] = "ready";
        } else if (action == "deploy") {
            result["deployment_id"] = plan.at("deployment_id");
            result["state"] = "deployed";
            result["configuration_released"] = next.at("configuration_session").is_null();
        } else if (action == "delete_mission") {
            result["mission_id"] = missionId;
            result["deleted"] = true;
        } else {
            const auto& mission = next.at("missions").as_object().at(missionId).as_object();
            result["mission_id"] = missionId;
            if (action != "assign") result["name"] = mission.at("name");
            if (action != "rename_mission") result["device_ids"] = assignment(mission);
            if (action == "add_mission") result["route_version"] = 0;
        }
        return result;
    }

    Object commitMockPlan(Object& next, const MockPlanCall& call, const Object& request,
                          const std::string& planId, const std::string& missionId,
                          const std::function<void(const Object&)>& publish) {
        const auto action = str(request, "action");
        if (action == "reopen_plan") next["configuration_session"] = nullptr;
        if (action == "validate" || action == "review") {
            auto& plan = next.at("plans").as_object().at(planId).as_object();
            if (action == "validate") {
                plan.at("validation").as_object()["validation_id"] = mockPlanStageId("check-", planId, plan);
                plan["review"] = nullptr;
            } else plan.at("review").as_object()["review_id"] = mockPlanStageId("review-", planId, plan);
        }
        renewMockConfiguration(next, call);
        return persistMockMcpResult(next, call, mockPlanResult(next, call, request, planId, missionId),
                                    mockPlanApprovalRequired(action), publish);
    }

    Object persistMockMcpResult(Object& next, const MockPlanCall& call, Object result, bool consumeApproval,
                                const std::function<void(const Object&)>& publish) {
        auto receipts = mock_mcp_draft_receipts_;
        const auto scope = call.scope();
        if (!receipts.contains(scope)) receipts[scope] = Object{};
        receipts.at(scope).as_object()[str(call.arguments, "request_id")] =
            Object{{"arguments", mockPlanFingerprint(call)}, {"result", result}};
        if (consumeApproval) {
            const char* used = "plan-write/approvals-used";
            if (!receipts.contains(used)) receipts[used] = Object{};
            receipts.at(used).as_object()[str(call.arguments, "approval_id")] = true;
        }
        const Object notification{{"type", "SecurityPlansChanged"}, {"payload", next}};
        persist(next, receipts);
        state_.swap(next);
        mock_mcp_draft_receipts_.swap(receipts);
        publish(notification);
        return result;
    }

public:
