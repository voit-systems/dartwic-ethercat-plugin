#pragma once

#include <sdk_api.h>

#include <cstdint>
#include <memory>

namespace Example {

/**
 * Custom transport example. The implementation is kept out of the header so
 * plugin registration does not pull the chosen networking library into every
 * plugin translation unit.
 */
class ExampleShareTransport final : public DARTWIC::API::ShareTransport {
public:
    explicit ExampleShareTransport(nlohmann::json config);
    ~ExampleShareTransport() override;

    void start(DARTWIC::Share::TransportCallbacks callbacks) override;
    DARTWIC::Share::Response request(const DARTWIC::Share::Request& request,
        std::chrono::milliseconds timeout) override;
    void publish(const DARTWIC::Share::Telemetry& telemetry) override;
    void control(const DARTWIC::Share::Control& control) override;
    void stop() override;

    uint64_t receivedCount() const;
    nlohmann::json lastFrame() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Example
