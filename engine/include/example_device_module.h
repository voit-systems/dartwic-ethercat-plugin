#pragma once

#include <modules/BaseModule.h>

namespace Example {
    class ExampleDeviceModule final : public DARTWIC::Modules::BaseModule {
    public:
        ExampleDeviceModule(nlohmann::json cfg, DARTWIC::API::SDK_API* api)
            : BaseModule(std::move(cfg), api) {}
    };
}
