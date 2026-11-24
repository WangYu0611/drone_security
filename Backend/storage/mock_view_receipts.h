#pragma once

#include <boost/json.hpp>
#include <functional>
#include <stdexcept>
#include <string>

struct MockViewError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Embedded atomically in each view's persistence file, stripped from snapshots.
class MockViewReceipts {
    using Object = boost::json::object;
public:
    struct Write {
        std::string key;
        Object arguments;
        std::function<Object(const Object&)> result;
        std::function<void()> validate;
    };

    void load(Object& document) {
        if (auto* ledger = document.if_contains("mock_mcp_view_receipts")) {
            receipts_ = std::move(ledger->as_object());
            document.erase("mock_mcp_view_receipts");
        }
    }

    const Object* lookup(const Write& request) const {
        const auto* value = receipts_.if_contains(request.key);
        if (!value) return nullptr;
        const auto& entry = value->as_object();
        if (entry.at("arguments") != request.arguments) throw MockViewError("REQUEST_ID_CONFLICT");
        return &entry.at("result").as_object();
    }

    template <typename Persist>
    Object commit(const Object& document, const Write* request, Persist persist) {
        auto stored = document;
        auto receipts = receipts_;
        Object result = request ? request->result(document) : document;
        if (request) receipts[request->key] = Object{{"arguments", request->arguments}, {"result", result}};
        if (!receipts.empty()) stored["mock_mcp_view_receipts"] = receipts;
        persist(stored);
        receipts_.swap(receipts);
        return result;
    }

private:
    Object receipts_;
};
