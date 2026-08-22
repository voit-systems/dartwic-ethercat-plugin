#pragma once

#include <plugins/BasePlugin.h>
#include "example_device_module.h"

namespace Example {
    class ExampleDevicePlugin final : public DARTWIC::Plugins::BasePlugin {
    public:
        ExampleDevicePlugin(nlohmann::json cfg, DARTWIC::API::SDK_API* api)
            : BasePlugin(std::move(cfg), api) {}

        void onPluginLoaded() override;

        DARTWIC::Modules::BaseModule* createModule(
            const std::string& module_type_id,
            nlohmann::json cfg,
            DARTWIC::API::SDK_API* api
        ) override;
    };
}
