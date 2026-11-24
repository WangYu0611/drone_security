    static bool mockPlanApprovalRequired(const std::string& action) {
        return action == "delete_plan" || action == "delete_mission" || action == "review" || action == "deploy";
    }

    void checkMockPlanApproval(const MockPlanCall& call) const {
        const auto id = str(call.arguments, "approval_id");
        if (id.empty()) throw PlanError("APPROVAL_REQUIRED", "Obtain operator approval for this action");
        if (id.size() > 64 || !std::all_of(id.begin(), id.end(), [](unsigned char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                   (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        })) throw PlanError("APPROVAL_INVALID", "The approval is invalid");

        const auto* used = mock_mcp_draft_receipts_.if_contains("plan-write/approvals-used");
        if (used && used->as_object().contains(id)) throw PlanError("APPROVAL_INVALID", "The approval was used");

        const auto filename = std::filesystem::path(path_).parent_path() / "mock_mcp_approvals" / (id + ".json");
        std::ifstream file(filename, std::ios::binary);
        std::string text(32769, '\0');
        file.read(text.data(), static_cast<std::streamsize>(text.size()));
        text.resize(static_cast<std::size_t>(file.gcount()));
        if (text.empty() || text.size() > 32768) throw PlanError("APPROVAL_INVALID", "The approval is unavailable");
        boost::system::error_code error;
        const auto approval = boost::json::parse(text, error);
        if (error || !approval.is_object()) throw PlanError("APPROVAL_INVALID", "The approval is invalid");
        const auto& record = approval.as_object();
        const auto now = OperationalContext::now();
        const auto* expires = record.if_contains("expires_at");
        const auto* arguments = record.if_contains("arguments");
        auto expected = mockPlanFingerprint(call);
        expected.erase("request_id");
        if (str(record, "identity") != call.identity || str(record, "tool") != call.tool ||
            !expires || !expires->is_number() || expires->to_number<double>() <= now ||
            expires->to_number<double>() > now + 300 || !arguments || *arguments != boost::json::value(expected))
            throw PlanError("APPROVAL_INVALID", "Obtain a new approval for this identity, operation and version");
    }

    static std::string mockPlanStageId(const char* prefix, const std::string& planId, const Object& plan) {
        return std::string(prefix) + planId + "-" + std::to_string(plan.at("version").to_number<int64_t>());
    }
