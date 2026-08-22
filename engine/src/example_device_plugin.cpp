#include "example_device_plugin.h"
#include "example_share_transport.h"
namespace Example {
    void ExampleDevicePlugin::onPluginLoaded() {
        dartwic->registerModuleType({
            .id = "example_device",
            .name = "Example Device"
        });

        dartwic->registerShareType({
            .id = "example_flight_link",
            .name = "Example Flight Link",
            .default_config = {
                {"node_name", "external-program"},
                {"receive_endpoint", "tcp://127.0.0.1:17600"},
                {"send_endpoint", "tcp://127.0.0.1:17601"}
            },
            .create = [](const nlohmann::json& config) {
                return std::make_shared<ExampleShareTransport>(config);
            }
        });

        DARTWIC::API::TaskTypeDefinition task;
        task.metadata.structure = DARTWIC::API::TaskStructure::Periodic;
        task.metadata.default_arguments = {{"message", "Hello from the example plugin"}};
        task.on_task = [](const auto&, auto&, double) {};
        dartwic->registerTaskType("example_task", "Example Task", std::move(task));

        dartwic->registerDCodeFunction(
            "test_value",
            "Test Value",
            [](const nlohmann::json& payload) {
                const double base = payload.is_object() ? payload.value("base", 40.0) : 40.0;
                const double offset = payload.is_object() ? payload.value("offset", 2.0) : 2.0;
                return nlohmann::json(base + offset);
            },
            "Return base + offset from the example plugin.",
            {
                {"base", "number", "Base value to add.", false},
                {"offset", "number", "Offset value to add.", false}
            },
            {
                {"value", "number", "The computed base + offset result.", true}
            }
        );

        dartwic->registerDCodeFunction(
            "test_outputs",
            "Test Outputs",
            [](const nlohmann::json& payload) {
                const double base = payload.is_object() ? payload.value("base", 40.0) : 40.0;
                const double offset = payload.is_object() ? payload.value("offset", 2.0) : 2.0;
                return nlohmann::json{
                    {"sum", base + offset},
                    {"difference", base - offset}
                };
            },
            "Return sum and difference fields from the example plugin.",
            {
                {"base", "number", "Base value used for both outputs.", false},
                {"offset", "number", "Offset value used for both outputs.", false}
            },
            {
                {"sum", "number", "The computed base + offset result.", true},
                {"difference", "number", "The computed base - offset result.", true}
            }
        );

        dartwic->registerOperation(
            "echo",
            "Echo",
            [](const nlohmann::json& payload) {
                return nlohmann::json{{"echo", payload}};
            }
        );

        dartwic->registerLoop(
            "heartbeat",
            "Example Heartbeat",
            DARTWIC::API::PluginLoopDefinition{
                .on_loop = []() {},
                .target_frequency_hz = 10.0
            }
        );
    }

    DARTWIC::Modules::BaseModule* ExampleDevicePlugin::createModule(
        const std::string& module_type_id,
        nlohmann::json cfg,
        DARTWIC::API::SDK_API* api
    ) {
        if (module_type_id != "example_device") {
            return nullptr;
        }

        return new ExampleDeviceModule(cfg, api);
    }
}

DARTWIC_PLUGIN_EXPORT DARTWIC::Plugins::BasePlugin* createPlugin(
    nlohmann::json cfg,
    DARTWIC::API::SDK_API* api
) {
    return new Example::ExampleDevicePlugin(cfg, api);
}
