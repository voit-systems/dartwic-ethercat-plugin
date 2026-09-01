#pragma once

#include "dartwic_ethercat_bridge.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace EtherCAT {
    class BridgeLibrary {
    public:
        struct Api;
        explicit BridgeLibrary(const std::filesystem::path& explicit_path = {});
        ~BridgeLibrary();
        BridgeLibrary(const BridgeLibrary&) = delete;
        BridgeLibrary& operator=(const BridgeLibrary&) = delete;

        nlohmann::json listAdapters() const;

        class Master {
        public:
            Master(const BridgeLibrary& library, const nlohmann::json& config);
            ~Master();
            Master(const Master&) = delete;
            Master& operator=(const Master&) = delete;

            nlohmann::json scan();
            void start();
            dw_ec_exchange_status exchange(std::span<const uint8_t> outputs,
                std::span<uint8_t> inputs);
            void stop() noexcept;
            size_t outputSize() const noexcept { return output_size_; }
            size_t inputSize() const noexcept { return input_size_; }

        private:
            const BridgeLibrary& library_;
            dw_ec_context* context_ = nullptr;
            size_t output_size_ = 0;
            size_t input_size_ = 0;
        };

    private:
        friend class Master;
        std::unique_ptr<Api> api_;
    };

    std::filesystem::path defaultBridgePath();
}
