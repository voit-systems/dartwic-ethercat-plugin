#include "ethercat_plugin.h"

#include "ethercat_codec.h"
#include "ethercat_module.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace EtherCAT {
namespace {
using DARTWIC::API::ChannelField;
using DARTWIC::API::ChannelStorage;

struct Mapping {
    std::string channel;
    bool channel_to_device = false;
    size_t bit_offset = 0;
    size_t bit_length = 0;
    ValueType type = ValueType::UInt16;
    double scale = 1.0;
    double offset = 0.0;
    int slave_position = -1;
    int index = -1;
    int subindex = -1;
};

struct CycleContext {
    std::shared_ptr<EthercatModule> module;
    std::vector<Mapping> outputs;
    std::vector<Mapping> inputs;
    DARTWIC::API::FixedChannelBatch output_channels;
    DARTWIC::API::FixedChannelBatch input_channels;
    DARTWIC::API::FixedChannelBatch diagnostic_channels;
    std::vector<double> output_values;
    std::vector<double> input_values;
    std::vector<double> diagnostic_values;
    std::vector<uint8_t> output_image;
    std::vector<uint8_t> input_image;
    std::string task_name;
    uint64_t consecutive_failures = 0;
};

std::string diagnosticChannel(const std::string& task, const char* suffix) {
    return task + ".ethercat." + suffix;
}

void createFixedChannel(DARTWIC::API::SDK_API* api, const std::string& channel, double initial = 0.0) {
    api->upsertChannelField(channel, ChannelField::VALUE, initial, ChannelStorage::Fixed);
}

std::vector<Mapping> parseMappings(const nlohmann::json& arguments) {
    if (!arguments.contains("mappings") || !arguments.at("mappings").is_array()) return {};
    std::vector<Mapping> mappings;
    for (const auto& value : arguments.at("mappings")) {
        if (!value.is_object()) throw std::invalid_argument("Each EtherCAT mapping must be an object.");
        Mapping mapping;
        mapping.channel = value.value("channel", std::string{});
        const auto direction = value.value("direction", std::string{});
        mapping.channel_to_device = direction == "channel_to_device";
        if (!mapping.channel_to_device && direction != "device_to_channel") {
            throw std::invalid_argument("EtherCAT mapping direction must be channel_to_device or device_to_channel.");
        }
        if (mapping.channel.empty()) throw std::invalid_argument("Every EtherCAT mapping requires a channel.");
        mapping.bit_offset = value.value("bit_offset", size_t{0});
        mapping.bit_length = value.value("bit_length", size_t{0});
        mapping.type = parseValueType(value.value("data_type", std::string{"uint16"}));
        mapping.scale = value.value("scale", 1.0);
        mapping.offset = value.value("offset", 0.0);
        mapping.slave_position = value.value("slave_position", -1);
        mapping.index = value.value("index", -1);
        mapping.subindex = value.value("subindex", -1);
        if (mapping.bit_length == 0 || mapping.bit_length > 64) {
            throw std::invalid_argument("EtherCAT PDO mapping bit length must be between 1 and 64.");
        }
        mappings.push_back(std::move(mapping));
    }
    return mappings;
}

std::shared_ptr<EthercatModule> moduleFor(DARTWIC::API::SDK_API* api, const std::string& name) {
    auto module = std::dynamic_pointer_cast<EthercatModule>(api->getModuleInstance(name));
    if (!module) throw std::runtime_error(
        "Configured EtherCAT module instance `" + name + "` is not available.");
    return module;
}

void validateMappings(const CycleContext& context) {
    const auto output_bits = context.output_image.size() * 8;
    const auto input_bits = context.input_image.size() * 8;
    for (const auto& mapping : context.outputs) {
        if (mapping.bit_offset + mapping.bit_length > output_bits) throw std::runtime_error(
            "Output PDO mapping for `" + mapping.channel + "` is outside the process image. Rescan and reselect its PDO entry.");
    }
    for (const auto& mapping : context.inputs) {
        if (mapping.bit_offset + mapping.bit_length > input_bits) throw std::runtime_error(
            "Input PDO mapping for `" + mapping.channel + "` is outside the process image. Rescan and reselect its PDO entry.");
    }
}

void configureCycle(DARTWIC::API::SDK_API* api, DARTWIC::API::TaskRuntime& runtime) {
    const auto& arguments = runtime.getArguments();
    const auto instance = arguments.value("module_instance_name", std::string{});
    if (instance.empty()) throw std::runtime_error("ethercat.cycle requires module_instance_name.");
    auto module = moduleFor(api, instance);
    const auto mappings = parseMappings(arguments);

    std::vector<std::string> fixed_inputs;
    for (const auto& mapping : mappings) {
        createFixedChannel(module->dartwic, mapping.channel);
        if (mapping.channel_to_device) fixed_inputs.push_back(mapping.channel);
    }
    runtime.setFixedInputChannels(std::move(fixed_inputs));
    createFixedChannel(module->dartwic, diagnosticChannel(runtime.getTaskName(), "exchange_time_us"));
    createFixedChannel(module->dartwic, diagnosticChannel(runtime.getTaskName(), "actual_wkc"));
    createFixedChannel(module->dartwic, diagnosticChannel(runtime.getTaskName(), "expected_wkc"));
    createFixedChannel(module->dartwic, diagnosticChannel(runtime.getTaskName(), "failure_count"));
}

std::shared_ptr<CycleContext> startCycle(DARTWIC::API::SDK_API* api,
    DARTWIC::API::TaskRuntime& runtime) {
    const auto& arguments = runtime.getArguments();
    auto context = std::make_shared<CycleContext>();
    context->task_name = runtime.getTaskName();
    context->module = moduleFor(api, arguments.value("module_instance_name", std::string{}));
    for (auto& mapping : parseMappings(arguments)) {
        (mapping.channel_to_device ? context->outputs : context->inputs).push_back(std::move(mapping));
    }
    if (context->outputs.empty() && context->inputs.empty()) {
        throw std::runtime_error(
            "Configure at least one EtherCAT PDO mapping before starting this task.");
    }

    context->module->start(context->task_name);
    try {
        context->output_image.resize(context->module->outputSize());
        context->input_image.resize(context->module->inputSize());
        validateMappings(*context);

        std::vector<std::string> output_names;
        std::vector<std::string> input_names;
        for (const auto& mapping : context->outputs) output_names.push_back(mapping.channel);
        for (const auto& mapping : context->inputs) input_names.push_back(mapping.channel);
        context->output_channels = context->module->dartwic->resolveFixedChannels(output_names);
        context->input_channels = context->module->dartwic->resolveFixedChannels(input_names);
        context->diagnostic_channels = context->module->dartwic->resolveFixedChannels({
            diagnosticChannel(context->task_name, "exchange_time_us"),
            diagnosticChannel(context->task_name, "actual_wkc"),
            diagnosticChannel(context->task_name, "expected_wkc"),
            diagnosticChannel(context->task_name, "failure_count"),
        });
        context->output_values.resize(context->outputs.size());
        context->input_values.resize(context->inputs.size());
        context->diagnostic_values.resize(4);
    } catch (...) {
        context->module->stop(context->task_name);
        throw;
    }
    return context;
}

uint64_t unixNanoseconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

void runCycle(DARTWIC::API::TaskRuntime& runtime) {
    const auto context = runtime.getTypedRuntimeContext<CycleContext>("ethercat.cycle");
    if (!context || !context->module || context->module->dartwic == nullptr) return;
    auto* api = context->module->dartwic;
    try {
        if (!context->output_channels.empty()) {
            api->queryFixedChannelValues(context->output_channels, context->output_values, 0.0);
            for (size_t i = 0; i < context->outputs.size(); ++i) {
                const auto& mapping = context->outputs[i];
                encodeValue(context->output_image, mapping.bit_offset, mapping.bit_length,
                    mapping.type, context->output_values[i], mapping.scale, mapping.offset);
            }
        }

        const auto status = context->module->exchange(context->output_image, context->input_image);
        const auto timestamp = unixNanoseconds();
        for (size_t i = 0; i < context->inputs.size(); ++i) {
            const auto& mapping = context->inputs[i];
            context->input_values[i] = decodeValue(context->input_image, mapping.bit_offset,
                mapping.bit_length, mapping.type, mapping.scale, mapping.offset);
        }
        if (!context->input_channels.empty()) {
            api->upsertFixedChannelValues(context->input_channels, context->input_values, timestamp);
        }
        context->consecutive_failures = 0;
        context->diagnostic_values = {
            static_cast<double>(status.exchange_duration_ns) / 1000.0,
            static_cast<double>(status.actual_wkc),
            static_cast<double>(status.expected_wkc),
            0.0,
        };
        api->upsertFixedChannelValues(context->diagnostic_channels, context->diagnostic_values, timestamp);
    } catch (...) {
        ++context->consecutive_failures;
        context->diagnostic_values[3] = static_cast<double>(context->consecutive_failures);
        api->upsertFixedChannelValues(context->diagnostic_channels, context->diagnostic_values, unixNanoseconds());
        if (context->consecutive_failures >= 3) throw;
    }
}

void stopCycle(DARTWIC::API::TaskRuntime& runtime) noexcept {
    const auto context = runtime.getTypedRuntimeContext<CycleContext>("ethercat.cycle");
    if (context && context->module) context->module->stop(context->task_name);
    runtime.removeRuntimeContext("ethercat.cycle");
}
} // namespace

void EthercatPlugin::onPluginLoaded() {
    dartwic->registerModuleType({
        .id = "master",
        .name = "EtherCAT Master",
    });

    dartwic->registerOperation("adapters", "List EtherCAT adapters", [](const nlohmann::json&) {
        BridgeLibrary bridge;
        return bridge.listAdapters();
    });
    dartwic->registerOperation("scan", "Scan EtherCAT bus", [this](const nlohmann::json& payload) {
        const auto instance = payload.value("module_instance_name", std::string{});
        if (instance.empty()) throw std::invalid_argument("scan requires module_instance_name.");
        return moduleFor(dartwic, instance)->scan();
    });

    DARTWIC::API::TaskTypeDefinition task;
    task.metadata.structure = DARTWIC::API::TaskStructure::Periodic;
    task.metadata.default_arguments = {
        {"module_instance_name", ""},
        {"mappings", nlohmann::json::array()},
    };
    task.on_configure = [this](const auto&, DARTWIC::API::TaskRuntime& runtime) {
        configureCycle(dartwic, runtime);
    };
    task.on_start = [this](const auto&, DARTWIC::API::TaskRuntime& runtime) {
        runtime.setTypedRuntimeContext("ethercat.cycle", startCycle(dartwic, runtime));
    };
    task.on_task = [](const auto&, DARTWIC::API::TaskRuntime& runtime, double) { runCycle(runtime); };
    task.on_end = [](const auto&, DARTWIC::API::TaskRuntime& runtime) { stopCycle(runtime); };
    task.cleanup = stopCycle;
    dartwic->registerTaskType("cycle", "EtherCAT Cyclic I/O", std::move(task));
}

DARTWIC::Modules::BaseModule* EthercatPlugin::createModule(const std::string& module_type_id,
    nlohmann::json config,
    DARTWIC::API::SDK_API* api) {
    if (module_type_id != "master") return nullptr;
    return new EthercatModule(std::move(config), api);
}
} // namespace EtherCAT

DARTWIC_PLUGIN_DECLARE_SDK_ABI()

DARTWIC_PLUGIN_EXPORT DARTWIC::Plugins::BasePlugin* createPlugin(
    nlohmann::json config,
    DARTWIC::API::SDK_API* api) {
    return new EtherCAT::EthercatPlugin(std::move(config), api);
}
