    static bool mockTranslationTool(const std::string& tool) {
        return tool == "preview_mock_plan_translation" || tool == "commit_mock_plan_translation" ||
               tool == "cancel_mock_plan_translation";
    }

    bool hasMockTranslationReceipt(const MockPlanCall& call, const std::string& tool,
                                   const std::string& preview) const {
        const auto* scope = mock_mcp_draft_receipts_.if_contains("plan-write/" + call.identity + "/" + tool);
        if (!scope) return false;
        for (const auto& entry : scope->as_object()) {
            const auto& result = entry.value().as_object().at("result").as_object();
            if (str(result, "preview_id") == preview) return true;
        }
        return false;
    }

    static void applyMockTranslation(Object& next, Object request, const Object& fixture,
                                     const Object& plan, Array& affected) {
        const auto& replacements = fixture.at("replacements").as_object();
        auto& paths = next.at("paths").as_object();
        Object updates, newKeys;
        for (const auto& id : plan.at("mission_ids").as_array()) {
            const auto& mission = next.at("missions").as_object().at(id.as_string()).as_object();
            const auto routeID = str(mission, "route_id");
            if (routeID.empty()) continue;
            auto& route = paths.at(routeID).as_object();
            if (str(route, "map_fixture_id") != str(fixture, "map_fixture_id") ||
                !route.contains("waypoint_fixture_ids"))
                throw PlanError("FIXTURE_NOT_FOUND", "A saved route has no matching map fixture");
            auto& points = route.at("waypoints").as_array();
            const auto& keys = route.at("waypoint_fixture_ids").as_array();
            if (keys.size() != points.size()) throw PlanError("FIXTURE_NOT_FOUND", "Saved route fixture changed");
            auto moved = route;
            auto& targets = moved.at("waypoints").as_array();
            Array translated;
            for (std::size_t i = 0; i < points.size(); ++i) {
                const auto* entry = replacements.if_contains(keys[i].as_string());
                if (!entry) throw PlanError("FIXTURE_NOT_FOUND", "A saved point has no authorized replacement");
                const auto& pair = entry->as_object();
                const auto& before = pair.at("before").as_object();
                const auto& after = pair.at("after").as_object();
                auto& previous = points[i].as_object();
                auto& replacement = targets[i].as_object();
                for (const auto* field : {"latitude", "longitude", "altitude"}) {
                    if (std::abs(number(previous, field) - number(before, field)) > 1e-8)
                        throw PlanError("PREVIEW_STALE", "A saved point no longer matches the approved fixture");
                    replacement[field] = after.at(field);
                }
                if (!previous.contains("location")) previous["location"] = before.at("location");
                replacement["location"] = after.at("location");
                translated.push_back(pair.at("target_id"));
            }
            updates[routeID] = std::move(moved);
            newKeys[routeID] = std::move(translated);
            affected.push_back(id);
        }
        request["paths"] = std::move(updates);
        movePlan(next, request, str(request, "plan_id"), "Mock MCP");
        for (const auto& entry : newKeys) paths.at(entry.key()).as_object()["waypoint_fixture_ids"] = entry.value();
    }

    Object writeMockTranslation(Object& next, const MockPlanCall& call,
                                const std::function<void(const Object&)>& publish) {
        const auto& args = call.arguments;
        const auto now = OperationalContext::now();
        const bool create = call.tool == "preview_mock_plan_translation";
        const bool commit = call.tool == "commit_mock_plan_translation";
        auto& sessions = next.at("edit_sessions").as_object();
        auto id = str(args, "preview_id");
        auto planID = str(args, "plan_id");
        const Object* preview = nullptr;
        if (!create) {
            const auto* value = sessions.if_contains(id);
            if (!value || !value->as_object().contains("mock_translation") ||
                str(value->as_object(), "owner_instance_id") != call.owner()) {
                if (hasMockTranslationReceipt(call, "commit_mock_plan_translation", id) ||
                    hasMockTranslationReceipt(call, "cancel_mock_plan_translation", id))
                    throw PlanError("INVALID_STATE", "The preview is closed");
                if (hasMockTranslationReceipt(call, "preview_mock_plan_translation", id))
                    throw PlanError("PREVIEW_EXPIRED", "Create a new preview");
                throw PlanError("NOT_FOUND", "Choose your own preview");
            }
            planID = str(value->as_object(), "plan_id");
            preview = &value->as_object().at("mock_translation").as_object();
            if (number(*preview, "expires_at") <= now) throw PlanError("PREVIEW_EXPIRED", "Create a new preview");
        }
        const auto* lease = activeConfiguration(next);
        const auto configuration = create ? str(args, "configuration_id") : str(*preview, "configuration_id");
        if (!lease || str(*lease, "owner_instance_id") != call.owner() || str(*lease, "plan_id") != planID ||
            mockConfigurationId(*lease) != configuration)
            throw PlanError("CONFIGURATION_EXPIRED", "Acquire the active Mock plan configuration");
        const auto& plan = lookup(next.at("plans").as_object(), planID, "PLAN_NOT_FOUND");
        if (plan.contains("real_assignment") || str(plan, "status") == "DEPLOYED")
            throw PlanError("INVALID_STATE", "Translate an editable Mock plan");
        const auto version = plan.at("version");
        Object request{{"action", create ? "begin_plan_move" : commit ? "move_plan" : "cancel_plan_move"},
                       {"instance_id", call.owner()}, {"plan_id", planID}, {"edit_session_id", id}};
        Object result{{"request_id", args.at("request_id")}, {"plan_id", planID}, {"confirmed_at", now}};
        if (create) {
            checkMockPlanVersion(args, plan, "expected_plan_version");
            const auto& fixture = args.at("resolved_translation_fixture");
            if (!fixture.is_object()) throw PlanError("FIXTURE_NOT_FOUND", "Choose an approved translation fixture");
            movePlan(next, request, planID, "Mock MCP");
            for (const auto& entry : sessions)
                if (str(entry.value().as_object(), "plan_id") == planID) id = std::string(entry.key());
            request["action"] = "move_plan";
            request["edit_session_id"] = id;
            auto dryRun = next;
            Array affected, checks;
            try {
                applyMockTranslation(dryRun, request, fixture.as_object(), plan, affected);
            } catch (const PlanError& error) {
                checks.emplace_back(Object{{"code", error.code}, {"message", error.code}, {"severity", "blocking"}});
            }
            const auto expires = now + 60;
            const bool ready = checks.empty();
            auto& session = sessions.at(id).as_object();
            session["mock_translation"] = Object{{"configuration_id", configuration}, {"base_plan_version", version},
                {"expires_at", expires}, {"fixture", fixture}, {"ready", ready}};
            result["preview_id"] = id;
            result["base_plan_version"] = version;
            result["state"] = ready ? "ready" : "blocked";
            result["affected_mission_ids"] = std::move(affected);
            result["checks"] = std::move(checks);
            result["expires_at"] = expires;
            renewMockConfiguration(next, call);
            session["lease_expires_at"] = expires;
        } else if (commit) {
            if (preview->at("base_plan_version") != version) throw PlanError("PREVIEW_STALE", "The plan changed");
            if (!preview->at("ready").as_bool()) throw PlanError("PREVIEW_BLOCKED", "Resolve preview checks first");
            checkMockPlanApproval(call);
            const auto fixture = preview->at("fixture").as_object();
            Array updated;
            applyMockTranslation(next, request, fixture, plan, updated);
            result["state"] = "committed";
            result["plan_version"] = plan.at("version");
            result["content_revision"] = plan.at("content_revision");
            result["updated_mission_ids"] = std::move(updated);
        } else {
            movePlan(next, request, planID, "Mock MCP");
            result["state"] = "canceled";
        }
        result["preview_id"] = id;
        next["version"] = state_.at("version").to_number<int64_t>() + 1;
        return persistMockMcpResult(next, call, std::move(result), commit, publish);
    }
