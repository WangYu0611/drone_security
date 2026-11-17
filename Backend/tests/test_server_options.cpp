#include <gtest/gtest.h>
#include "core/server_options.h"

#include <initializer_list>
#include <vector>

namespace {
ServerOptions Parse(std::initializer_list<const char*> arguments)
{
    std::vector<const char*> argv{"DroneBackend"};
    argv.insert(argv.end(), arguments.begin(), arguments.end());
    return ParseServerOptions(static_cast<int>(argv.size()), argv.data());
}
} // namespace

TEST(ServerOptionsTest, DefaultsPreserveExistingConfigInvocation)
{
    const auto defaults = Parse({});
    EXPECT_EQ(defaults.config_path, "config.yaml");
    EXPECT_FALSE(defaults.http_port);
    EXPECT_FALSE(defaults.ws_port);
    EXPECT_EQ(Parse({"C:/Deployment Config/backend.yaml"}).config_path,
              "C:/Deployment Config/backend.yaml");

    AppConfig config;
    config.http_port = 19080;
    config.ws_port = 19081;
    ApplyServerOptions(config, defaults);
    EXPECT_EQ(config.http_port, 19080);
    EXPECT_EQ(config.ws_port, 19081);
}

TEST(ServerOptionsTest, AcceptsConfigAndBothOptionFormsInEitherOrder)
{
    const auto options = Parse({"deployment.yaml", "--ws-port=19081", "--http-port", "19080"});
    EXPECT_EQ(options.config_path, "deployment.yaml");
    EXPECT_EQ(options.http_port, 19080);
    EXPECT_EQ(options.ws_port, 19081);

    const auto defaults = Parse({"--http-port=1", "--ws-port", "65535"});
    EXPECT_EQ(defaults.config_path, "config.yaml");
    EXPECT_EQ(defaults.http_port, 1);
    EXPECT_EQ(defaults.ws_port, 65535);
}

TEST(ServerOptionsTest, RejectsUnknownExtraAndDuplicateArguments)
{
    EXPECT_THROW(Parse({"--unknown", "80"}), std::invalid_argument);
    EXPECT_THROW(Parse({"config.yaml", "second.yaml"}), std::invalid_argument);
    EXPECT_THROW(Parse({"--http-port", "80", "config.yaml"}), std::invalid_argument);
    EXPECT_THROW(Parse({""}), std::invalid_argument);
    EXPECT_THROW(Parse({"--http-port", "80", "--http-port=81"}), std::invalid_argument);
    EXPECT_THROW(Parse({"--ws-port=81", "--ws-port", "82"}), std::invalid_argument);
}

TEST(ServerOptionsTest, RejectsMissingValues)
{
    EXPECT_THROW(Parse({"--http-port"}), std::invalid_argument);
    EXPECT_THROW(Parse({"--ws-port"}), std::invalid_argument);
    EXPECT_THROW(Parse({"--http-port", "--ws-port", "81"}), std::invalid_argument);
    EXPECT_THROW(Parse({"--ws-port="}), std::invalid_argument);
}

TEST(ServerOptionsTest, RejectsInvalidAndOutOfRangePorts)
{
    for (const char* value : {"", "0", "-1", "65536", "99999999999999999999",
                              "8080x", "3.5", " 80", "80 ", "+80"}) {
        SCOPED_TRACE(value);
        EXPECT_THROW(Parse({"--http-port", value}), std::invalid_argument);
        EXPECT_THROW(Parse({"--ws-port", value}), std::invalid_argument);
    }
}

TEST(ServerOptionsTest, RejectsExplicitAndEffectiveListenerConflicts)
{
    EXPECT_THROW(Parse({"--http-port", "8080", "--ws-port=8080"}), std::invalid_argument);
    AppConfig config;
    EXPECT_THROW(ApplyServerOptions(config, Parse({"--http-port=8081"})), std::invalid_argument);
    EXPECT_EQ(config.http_port, 8080);
    EXPECT_EQ(config.ws_port, 8081);
    EXPECT_THROW(ApplyServerOptions(config, Parse({"--ws-port=8080"})), std::invalid_argument);
    config.http_port = config.ws_port;
    EXPECT_THROW(ApplyServerOptions(config, Parse({})), std::invalid_argument);
    config.http_port = 0;
    EXPECT_THROW(ApplyServerOptions(config, Parse({})), std::invalid_argument);
}

TEST(ServerOptionsTest, OverridesOnlyRequestedListenerAndPreservesAircraftSettings)
{
    AppConfig config;
    config.jetson_host = "192.0.2.10";
    config.storage_path = "operator/registry.json";
    config.heartbeat_hz = 7;
    config.command_repeat_count = 9;
    PortMapping mapping;
    mapping.slot = 1;
    mapping.jetson_host = "192.0.2.20";
    mapping.send_port = 8889;
    mapping.recv_port = 8888;
    mapping.ros_topic_prefix = "/operator";
    config.port_map[1] = mapping;

    ApplyServerOptions(config, Parse({"--http-port", "19080"}));
    EXPECT_EQ(config.http_port, 19080);
    EXPECT_EQ(config.ws_port, 8081);
    EXPECT_EQ(config.jetson_host, "192.0.2.10");
    EXPECT_EQ(config.storage_path, "operator/registry.json");
    EXPECT_EQ(config.heartbeat_hz, 7);
    EXPECT_EQ(config.command_repeat_count, 9);
    ASSERT_EQ(config.port_map.size(), 1u);
    EXPECT_EQ(config.port_map.at(1).jetson_host, "192.0.2.20");
    EXPECT_EQ(config.port_map.at(1).send_port, 8889);
    EXPECT_EQ(config.port_map.at(1).recv_port, 8888);
    EXPECT_EQ(config.port_map.at(1).ros_topic_prefix, "/operator");
}
