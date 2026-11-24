    static bool mockRouteAction(const std::string& action) {
        return action == "begin_route_edit" || action == "save_route" ||
               action == "finish_route_edit" || action == "discard_route_edit";
    }

    static int64_t mockRouteVersion(const Object& doc, const Object& mission) {
        const auto* path = doc.at("paths").as_object().if_contains(str(mission, "route_id"));
        if (!path) return 0;
        const auto* revision = path->as_object().if_contains("revision");
        return revision ? revision->to_number<int64_t>() : 1;
    }

    static Object mockRouteRequest(const Object& doc, const MockPlanCall& call, const std::string& action) {
        const auto& args = call.arguments;
        auto planId = str(args, "plan_id");
        auto missionId = str(args, "mission_id");
        const auto editId = str(args, "route_edit_id");
        if (action != "begin_route_edit") {
            const auto* handle = doc.at("edit_sessions").as_object().if_contains(editId);
            if (!handle || str(handle->as_object(), "owner_instance_id") != call.owner() ||
                str(handle->as_object(), "mode") == "MOVE")
                throw PlanError("CONFIGURATION_EXPIRED", "Acquire your current route edit session");
            planId = str(handle->as_object(), "plan_id");
            missionId = str(handle->as_object(), "mission_id");
        }
        const auto* lease = activeConfiguration(doc);
        if (!lease || str(*lease, "owner_instance_id") != call.owner() || str(*lease, "plan_id") != planId ||
            (action == "begin_route_edit" && mockConfigurationId(*lease) != str(args, "configuration_id")))
            throw PlanError("CONFIGURATION_EXPIRED", "Acquire your plan configuration before editing its route");
        const auto& plan = lookup(doc.at("plans").as_object(), planId, "PLAN_NOT_FOUND");
        if (plan.contains("real_assignment")) throw PlanError("PLAN_NOT_FOUND", "Unknown Mock plan");
        const auto& mission = lookup(doc.at("missions").as_object(), missionId, "MISSION_NOT_FOUND");
        if (str(mission, "plan_id") != planId) throw PlanError("MISSION_NOT_FOUND", "Unknown Mock mission");
        if (action != "discard_route_edit") {
            const auto* expected = args.if_contains("expected_route_version");
            if (!expected || (!expected->is_int64() && !expected->is_uint64()) ||
                *expected != boost::json::value(mockRouteVersion(doc, mission)))
                throw PlanError("VERSION_CONFLICT", "Read the current saved route version");
        }
        Object request{{"action", action}, {"instance_id", call.owner()}, {"plan_id", planId},
                       {"mission_id", missionId}, {"expected_version", doc.at("version")}};
        if (action == "begin_route_edit") {
            request["workflow"] = true;
            request["content_revision"] = plan.at("content_revision");
        } else request["edit_session_id"] = editId;
        if (action == "save_route") {
            const auto* path = args.if_contains("resolved_fixture_path");
            if (!path || !path->is_object()) throw PlanError("FIXTURE_NOT_FOUND", "Choose an authorized route fixture");
            request["path"] = *path;
            request["keep_editing"] = true;
        }
        return request;
    }

    static Object mockRouteResult(const Object& next, const MockPlanCall& call, const Object& request,
                                  const std::string& planId, const std::string& missionId) {
        const auto action = str(request, "action");
        const auto& mission = next.at("missions").as_object().at(missionId).as_object();
        Object result{{"request_id", call.arguments.at("request_id")}, {"confirmed_at", OperationalContext::now()},
                      {"mission_id", missionId}, {"route_version", mockRouteVersion(next, mission)}};
        if (action == "begin_route_edit") {
            for (const auto& item : next.at("edit_sessions").as_object()) {
                const auto& edit = item.value().as_object();
                if (str(edit, "owner_instance_id") == call.owner() && str(edit, "mission_id") == missionId) {
                    result["plan_id"] = planId;
                    result["route_edit_id"] = item.key();
                    result["lease_expires_at"] = edit.at("lease_expires_at");
                    return result;
                }
            }
            throw PlanError("CONFIGURATION_EXPIRED", "No active route editor");
        }
        result["route_edit_id"] = call.arguments.at("route_edit_id");
        result["edit_open"] = action == "save_route";
        if (action == "save_route") {
            const auto& route = next.at("paths").as_object().at(str(mission, "route_id")).as_object();
            result["content_revision"] = next.at("plans").as_object().at(planId).as_object().at("content_revision");
            result["state"] = "saved";
            result["waypoint_count"] = route.at("waypoints").as_array().size();
        } else result["disposition"] = call.arguments.at("disposition");
        return result;
    }
