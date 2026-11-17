#pragma once

#include "core/config_loader.h"

#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

struct ServerOptions {
    std::string config_path = "config.yaml";
    std::optional<int> http_port;
    std::optional<int> ws_port;
};

namespace server_options_detail {
inline int ParsePort(std::string_view option, std::string_view value)
{
    int port = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), port);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size() || port < 1 || port > 65535) {
        throw std::invalid_argument(std::string(option) + " requires an integer port from 1 to 65535");
    }
    return port;
}

inline void ValidatePorts(int http_port, int ws_port)
{
    if (http_port < 1 || http_port > 65535 || ws_port < 1 || ws_port > 65535) {
        throw std::invalid_argument("HTTP and WebSocket ports must be from 1 to 65535");
    }
    if (http_port == ws_port) {
        throw std::invalid_argument("HTTP and WebSocket ports must be different");
    }
}
} // namespace server_options_detail

// Keep the existing optional first config-file argument. Overrides affect only
// the two TCP listeners; all aircraft and execution settings stay in that file.
inline ServerOptions ParseServerOptions(int argc, const char* const* argv)
{
    ServerOptions options;
    int index = 1;
    if (index < argc && std::string_view(argv[index]).find('-') != 0) {
        options.config_path = argv[index++];
        if (options.config_path.empty()) {
            throw std::invalid_argument("Configuration path must not be empty");
        }
    }
    for (; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        const auto equals = argument.find('=');
        const auto name = argument.substr(0, equals);
        std::optional<int>* destination = nullptr;
        if (name == "--http-port") destination = &options.http_port;
        else if (name == "--ws-port") destination = &options.ws_port;
        else throw std::invalid_argument("Unknown argument: " + std::string(argument));

        if (destination->has_value()) {
            throw std::invalid_argument("Duplicate option: " + std::string(name));
        }
        std::string_view value;
        if (equals != std::string_view::npos) {
            value = argument.substr(equals + 1);
        } else {
            if (++index >= argc || std::string_view(argv[index]).find("--") == 0) {
                throw std::invalid_argument("Missing value for " + std::string(name));
            }
            value = argv[index];
        }
        *destination = server_options_detail::ParsePort(name, value);
    }
    if (options.http_port && options.ws_port) {
        server_options_detail::ValidatePorts(*options.http_port, *options.ws_port);
    }
    return options;
}

inline void ApplyServerOptions(AppConfig& config, const ServerOptions& options)
{
    const int http_port = options.http_port.value_or(config.http_port);
    const int ws_port = options.ws_port.value_or(config.ws_port);
    server_options_detail::ValidatePorts(http_port, ws_port);
    config.http_port = http_port;
    config.ws_port = ws_port;
}
