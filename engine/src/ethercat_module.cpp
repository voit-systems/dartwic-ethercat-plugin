#include "ethercat_module.h"

#include <stdexcept>

namespace EtherCAT {
EthercatModule::EthercatModule(nlohmann::json config, DARTWIC::API::SDK_API* api)
    : BaseModule(std::move(config), api),
      instance_name_(getConfig<std::string>("name")) {}

nlohmann::json EthercatModule::bridgeConfig() const {
    return {
        {"mode", getParameter<std::string>("mode", "simulator")},
        {"adapter", getParameter<std::string>("adapter", "")},
        {"simulator_profile", getParameter<std::string>("simulator_profile", "standard_io")},
        {"receive_timeout_us", getParameter<int>("receive_timeout_us", 500)},
    };
}

nlohmann::json EthercatModule::listAdapters() {
    std::scoped_lock lock(mutex_);
    if (!bridge_) bridge_ = std::make_unique<BridgeLibrary>();
    return bridge_->listAdapters();
}

nlohmann::json EthercatModule::scan() {
    std::scoped_lock lock(mutex_);
    if (!task_owner_.empty()) throw std::runtime_error(
        "Cannot scan EtherCAT module `" + instance_name_ + "` while task `" + task_owner_ + "` owns it.");
    if (!bridge_) bridge_ = std::make_unique<BridgeLibrary>();
    auto candidate = std::make_unique<BridgeLibrary::Master>(*bridge_, bridgeConfig());
    cached_topology_ = candidate->scan();
    return cached_topology_;
}

nlohmann::json EthercatModule::start(const std::string& task_name) {
    std::scoped_lock lock(mutex_);
    if (!task_owner_.empty() && task_owner_ != task_name) throw std::runtime_error(
        "EtherCAT module `" + instance_name_ + "` is already owned by task `" + task_owner_ + "`.");
    if (!bridge_) bridge_ = std::make_unique<BridgeLibrary>();
    auto candidate = std::make_unique<BridgeLibrary::Master>(*bridge_, bridgeConfig());
    cached_topology_ = candidate->scan();
    candidate->start();
    master_ = std::move(candidate);
    task_owner_ = task_name;
    return cached_topology_;
}

dw_ec_exchange_status EthercatModule::exchange(std::span<const uint8_t> outputs,
    std::span<uint8_t> inputs) {
    std::scoped_lock lock(mutex_);
    if (!master_) throw std::runtime_error("EtherCAT master is not running.");
    return master_->exchange(outputs, inputs);
}

void EthercatModule::stop(const std::string& task_name) noexcept {
    std::scoped_lock lock(mutex_);
    if (task_owner_ != task_name) return;
    if (master_) master_->stop();
    master_.reset();
    task_owner_.clear();
}

bool EthercatModule::isOwnedBy(const std::string& task_name) const {
    std::scoped_lock lock(mutex_);
    return task_owner_ == task_name;
}

size_t EthercatModule::outputSize() const {
    std::scoped_lock lock(mutex_);
    return master_ ? master_->outputSize() : 0;
}

size_t EthercatModule::inputSize() const {
    std::scoped_lock lock(mutex_);
    return master_ ? master_->inputSize() : 0;
}
} // namespace EtherCAT
