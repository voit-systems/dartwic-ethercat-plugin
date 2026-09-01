#pragma once

#include <plugins/BasePlugin.h>

namespace EtherCAT {
    class EthercatPlugin final : public DARTWIC::Plugins::BasePlugin {
    public:
        using BasePlugin::BasePlugin;
        void onPluginLoaded() override;
        DARTWIC::Modules::BaseModule* createModule(
            const std::string& module_type_id,
            nlohmann::json config,
            DARTWIC::API::SDK_API* api) override;
    };
}
