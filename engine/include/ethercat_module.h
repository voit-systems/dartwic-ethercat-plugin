#pragma once

#include "ethercat_bridge_loader.h"

#include <modules/BaseModule.h>

#include <memory>
#include <mutex>
#include <span>
#include <string>

namespace EtherCAT {
    class EthercatModule final : public DARTWIC::Modules::BaseModule {
    public:
        EthercatModule(nlohmann::json config, DARTWIC::API::SDK_API* api);

        const std::string& instanceName() const noexcept { return instance_name_; }
        nlohmann::json bridgeConfig() const;
        nlohmann::json listAdapters();
        nlohmann::json scan();
        nlohmann::json start(const std::string& task_name);
        dw_ec_exchange_status exchange(std::span<const uint8_t> outputs,
            std::span<uint8_t> inputs);
        void stop(const std::string& task_name) noexcept;
        bool isOwnedBy(const std::string& task_name) const;
        size_t outputSize() const;
        size_t inputSize() const;

    private:
        std::string instance_name_;
        std::filesystem::path bridge_path_;
        mutable std::mutex mutex_;
        std::string task_owner_;
        std::unique_ptr<BridgeLibrary> bridge_;
        std::unique_ptr<BridgeLibrary::Master> master_;
        nlohmann::json cached_topology_ = nlohmann::json::object();
    };
}
