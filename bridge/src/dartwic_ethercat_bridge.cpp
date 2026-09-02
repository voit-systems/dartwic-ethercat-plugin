#include "dartwic_ethercat_bridge.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "kickcat/AbstractSocket.h"
#include "kickcat/Bus.h"
#include "kickcat/CoE/OD.h"
#include "kickcat/ESC/EmulatedESC.h"
#include "kickcat/ESI/Device.h"
#include "kickcat/ESI/SIIBuilder.h"
#include "kickcat/Link.h"
#include "kickcat/LoopbackSocket.h"
#include "kickcat/PDO.h"
#include "kickcat/SocketNull.h"
#include "kickcat/helpers.h"
#include "kickcat/slave/Slave.h"

using namespace std::chrono_literals;

struct dw_ec_context {
    nlohmann::json config;
    std::string error;
    nlohmann::json topology = nlohmann::json::object();
    std::shared_ptr<kickcat::AbstractSocket> nominal_socket;
    std::shared_ptr<kickcat::AbstractSocket> redundant_socket;
    std::shared_ptr<kickcat::Link> link;
    std::unique_ptr<kickcat::Bus> bus;
    std::vector<uint8_t> iomap;
    size_t input_size = 0;
    size_t output_size = 0;
    uint32_t expected_wkc = 0;
    uint64_t cycle_count = 0;
    bool scanned = false;
    bool running = false;

    std::unique_ptr<kickcat::EmulatedESC> sim_esc;
    std::unique_ptr<kickcat::PDO> sim_pdo;
    std::unique_ptr<kickcat::slave::Slave> sim_slave;
    std::vector<uint8_t> sim_inputs;
    std::vector<uint8_t> sim_outputs;
    bool sim_op_phase = false;
};

namespace {
using nlohmann::json;

dw_ec_result guard(dw_ec_context* context, const std::function<void()>& action) {
    try {
        action();
        if (context != nullptr) context->error.clear();
        return DW_EC_OK;
    } catch (const std::exception& error) {
        if (context != nullptr) context->error = error.what();
        return DW_EC_INTERNAL_ERROR;
    } catch (...) {
        if (context != nullptr) context->error = "Unknown EtherCAT bridge error.";
        return DW_EC_INTERNAL_ERROR;
    }
}

dw_ec_result writeJson(const json& value, char* destination, size_t destination_size,
    size_t* required_size) {
    if (required_size == nullptr) return DW_EC_INVALID_ARGUMENT;
    const auto text = value.dump();
    *required_size = text.size() + 1;
    if (destination == nullptr || destination_size == 0) return DW_EC_OK;
    if (destination_size < *required_size) return DW_EC_INVALID_ARGUMENT;
    std::memcpy(destination, text.c_str(), *required_size);
    return DW_EC_OK;
}

kickcat::ESI::Device makeStandardDevice() {
    using namespace kickcat;
    ESI::Device device;
    device.type = "DARTWIC-SIM-IO";
    device.name = "DARTWIC EtherCAT Simulator";
    device.vendor_name = "DARTWIC";
    device.vendor_id = 0x00DA47u;
    device.product_code = 0x00000001u;
    device.revision_no = 0x00010000u;
    device.serial_no = 1;

    ESI::SmInfo output_sm;
    output_sm.type = SyncManager::Output;
    output_sm.start_address = 0x1800;
    output_sm.default_size = 13;
    output_sm.control_byte = 0x64;
    output_sm.enable = 1;
    device.sync_managers.push_back(output_sm);

    ESI::SmInfo input_sm;
    input_sm.type = SyncManager::Input;
    input_sm.start_address = 0x1C00;
    input_sm.default_size = 17;
    input_sm.control_byte = 0x20;
    input_sm.enable = 1;
    device.sync_managers.push_back(input_sm);

    ESI::Fmmu output_fmmu;
    output_fmmu.type = fmmu::Outputs;
    device.fmmus.push_back(output_fmmu);
    ESI::Fmmu input_fmmu;
    input_fmmu.type = fmmu::Inputs;
    device.fmmus.push_back(input_fmmu);

    ESI::Pdo rx;
    rx.index = 0x1600;
    rx.name = "Command outputs";
    rx.sm = 0;
    rx.fixed = true;
    rx.entries = {
        {0x7000, 1, 32, "Command U32", "", "UDINT"},
        {0x7000, 2, 64, "Command F64", "", "LREAL"},
        {0x7000, 3, 1, "Command Bool", "", "BOOL"},
        {0x0000, 0, 7, "Padding", "", ""},
    };
    device.rx_pdos.push_back(rx);

    ESI::Pdo tx;
    tx.index = 0x1A00;
    tx.name = "Measured inputs";
    tx.sm = 1;
    tx.fixed = true;
    tx.entries = {
        {0x6000, 1, 32, "Echo U32", "", "UDINT"},
        {0x6000, 2, 64, "Echo F64", "", "LREAL"},
        {0x6000, 3, 1, "Echo Bool", "", "BOOL"},
        {0x0000, 0, 7, "Padding", "", ""},
        {0x6000, 4, 32, "Cycle Counter", "", "UDINT"},
    };
    device.tx_pdos.push_back(tx);

    ESI::Eeprom eeprom;
    eeprom.byte_size = 2048;
    eeprom.config_data = {0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    device.eeprom = std::move(eeprom);
    return device;
}

std::string dataType(uint8_t code, uint8_t bits) {
    using kickcat::CoE::DataType;
    switch (static_cast<DataType>(code)) {
        case DataType::BOOLEAN: return "bool";
        case DataType::INTEGER8: return "int8";
        case DataType::INTEGER16: return "int16";
        case DataType::INTEGER32: return "int32";
        case DataType::INTEGER64: return "int64";
        case DataType::UNSIGNED8: return "uint8";
        case DataType::UNSIGNED16: return "uint16";
        case DataType::UNSIGNED32: return "uint32";
        case DataType::UNSIGNED64: return "uint64";
        case DataType::REAL32: return "float32";
        case DataType::REAL64: return "float64";
        default:
            if (bits == 1) return "bool";
            if (bits <= 8) return "uint8";
            if (bits <= 16) return "uint16";
            if (bits <= 32) return "uint32";
            return "uint64";
    }
}

json pdoEntries(const kickcat::Slave& slave, bool inputs, size_t process_image_bit_base) {
    const auto& pdos = inputs ? slave.sii.TxPDO : slave.sii.RxPDO;
    json result = json::array();
    size_t cursor = process_image_bit_base;
    for (const auto& pdo : pdos) {
        for (const auto& entry : pdo.entries) {
            if (entry.index != 0) {
                result.push_back({
                    {"pdo_index", pdo.index},
                    {"index", entry.index},
                    {"subindex", entry.subindex},
                    {"name", std::string(slave.sii.getString(entry.name))},
                    {"data_type", dataType(entry.data_type, entry.bitlen)},
                    {"bit_offset", cursor},
                    {"bit_length", entry.bitlen},
                });
            }
            cursor += entry.bitlen;
        }
    }
    return result;
}

void buildTopology(dw_ec_context& context) {
    json slaves = json::array();
    context.input_size = 0;
    context.output_size = 0;
    context.expected_wkc = 0;
    for (const auto& slave : context.bus->slaves()) {
        context.input_size += static_cast<size_t>(std::max(slave.input.bsize, 0));
        context.output_size += static_cast<size_t>(std::max(slave.output.bsize, 0));
        if (slave.input.bsize > 0) context.expected_wkc += 1;
        if (slave.output.bsize > 0) context.expected_wkc += 2;
    }
    for (size_t position = 0; position < context.bus->slaves().size(); ++position) {
        const auto& slave = context.bus->slaves()[position];
        const size_t input_base = slave.input.data == nullptr ? 0 :
            static_cast<size_t>(slave.input.data - context.iomap.data()) * 8;
        const size_t output_base = slave.output.data == nullptr ? 0 :
            (static_cast<size_t>(slave.output.data - context.iomap.data()) - context.input_size) * 8;
        slaves.push_back({
            {"position", position},
            {"name", slave.name()},
            {"type", slave.type()},
            {"station_address", slave.address},
            {"vendor_id", slave.sii.info.vendor_id},
            {"product_code", slave.sii.info.product_code},
            {"revision", slave.sii.info.revision_number},
            {"serial", slave.sii.info.serial_number},
            {"inputs", pdoEntries(slave, true, input_base)},
            {"outputs", pdoEntries(slave, false, output_base)},
        });
    }
    context.topology = {
        {"process_image", {{"inputs_bytes", context.input_size}, {"outputs_bytes", context.output_size}}},
        {"slaves", std::move(slaves)},
    };
}

void createSimulator(dw_ec_context& context) {
    using namespace kickcat;
    auto device = makeStandardDevice();
    context.sim_esc = std::make_unique<EmulatedESC>();
    context.sim_esc->loadEeprom(ESI::buildEepromImage(device));
    context.sim_pdo = std::make_unique<PDO>(context.sim_esc.get());
    context.sim_slave = std::make_unique<slave::Slave>(context.sim_esc.get(), context.sim_pdo.get());
    context.sim_inputs.assign(17, 0);
    context.sim_outputs.assign(13, 0);
    context.sim_pdo->setInput(context.sim_inputs.data(), static_cast<uint32_t>(context.sim_inputs.size()));
    context.sim_pdo->setOutput(context.sim_outputs.data(), static_cast<uint32_t>(context.sim_outputs.size()));
    context.sim_slave->start();

    auto tick = [&context]() {
        context.sim_slave->routine();
        if (context.sim_op_phase && context.sim_slave->state() == State::SAFE_OP) {
            context.sim_slave->validateOutputData();
        }
        std::copy(context.sim_outputs.begin(), context.sim_outputs.end(), context.sim_inputs.begin());
        const uint32_t counter = static_cast<uint32_t>(context.cycle_count);
        std::memcpy(context.sim_inputs.data() + 13, &counter, sizeof(counter));
    };
    context.nominal_socket = std::make_shared<LoopbackSocket>(
        std::vector<EmulatedESC*>{context.sim_esc.get()}, std::move(tick));
    context.redundant_socket = std::make_shared<SocketNull>();
}

void scanBus(dw_ec_context& context) {
    using namespace kickcat;
    const auto mode = context.config.value("mode", std::string{"simulator"});
    if (mode == "simulator") {
        createSimulator(context);
    } else if (mode == "hardware") {
        const auto adapter = context.config.value("adapter", std::string{});
        if (adapter.empty()) throw std::invalid_argument("Hardware mode requires an adapter.");
        auto sockets = createSockets(adapter, "");
        context.nominal_socket = std::get<0>(sockets);
        context.redundant_socket = std::get<1>(sockets);
    } else {
        throw std::invalid_argument("EtherCAT bridge mode must be simulator or hardware.");
    }

    context.link = std::make_shared<Link>(context.nominal_socket, context.redundant_socket, []() {});
    context.link->setTimeout(std::chrono::microseconds(
        std::max(context.config.value("receive_timeout_us", 500), 50)));
    context.bus = std::make_unique<Bus>(context.link);
    context.bus->init(100ms);
    context.iomap.assign(65536, 0);
    context.bus->createMapping(context.iomap.data(), context.iomap.size());
    context.bus->requestState(State::SAFE_OP);
    context.bus->waitForState(State::SAFE_OP, 500ms);
    buildTopology(context);
    context.scanned = true;
}
} // namespace

extern "C" {
uint32_t DW_EC_BRIDGE_CALL dw_ec_bridge_abi_version(void) { return DW_EC_BRIDGE_ABI_VERSION; }

dw_ec_result DW_EC_BRIDGE_CALL dw_ec_list_adapters_json(char* destination,
    size_t destination_size,
    size_t* required_size) {
    try {
        json adapters = json::array({{{"id", "simulator"}, {"name", "Built-in KickCAT simulator"}, {"kind", "simulator"}}});
        try {
            for (const auto& adapter : kickcat::listInterfaces()) {
                adapters.push_back({{"id", adapter.name}, {"name", adapter.description.empty() ? adapter.name : adapter.description}, {"kind", "hardware"}});
            }
        } catch (const std::exception&) {
            // Npcap is optional for simulator-only Windows installations.
        }
        return writeJson(adapters, destination, destination_size, required_size);
    } catch (...) {
        return DW_EC_INTERNAL_ERROR;
    }
}

dw_ec_result DW_EC_BRIDGE_CALL dw_ec_open(const char* config_json, dw_ec_context** context) {
    if (config_json == nullptr || context == nullptr) return DW_EC_INVALID_ARGUMENT;
    try {
        auto candidate = std::make_unique<dw_ec_context>();
        candidate->config = json::parse(config_json);
        *context = candidate.release();
        return DW_EC_OK;
    } catch (...) {
        return DW_EC_INVALID_ARGUMENT;
    }
}

void DW_EC_BRIDGE_CALL dw_ec_close(dw_ec_context* context) { delete context; }

dw_ec_result DW_EC_BRIDGE_CALL dw_ec_scan(dw_ec_context* context) {
    if (context == nullptr) return DW_EC_INVALID_ARGUMENT;
    return guard(context, [&]() { scanBus(*context); });
}

dw_ec_result DW_EC_BRIDGE_CALL dw_ec_topology_json(dw_ec_context* context,
    char* destination,
    size_t destination_size,
    size_t* required_size) {
    if (context == nullptr || !context->scanned) return DW_EC_NOT_READY;
    return writeJson(context->topology, destination, destination_size, required_size);
}

dw_ec_result DW_EC_BRIDGE_CALL dw_ec_process_image_sizes(dw_ec_context* context,
    size_t* output_size,
    size_t* input_size) {
    if (context == nullptr || output_size == nullptr || input_size == nullptr) return DW_EC_INVALID_ARGUMENT;
    if (!context->scanned) return DW_EC_NOT_READY;
    *output_size = context->output_size;
    *input_size = context->input_size;
    return DW_EC_OK;
}

dw_ec_result DW_EC_BRIDGE_CALL dw_ec_start(dw_ec_context* context) {
    if (context == nullptr) return DW_EC_INVALID_ARGUMENT;
    if (!context->scanned) return DW_EC_NOT_READY;
    return guard(context, [&]() {
        auto error = [](kickcat::DatagramState const&) {};
        context->bus->processDataReadWrite(error);
        context->sim_op_phase = true;
        context->bus->requestState(kickcat::State::OPERATIONAL);
        context->bus->waitForState(kickcat::State::OPERATIONAL, 500ms, [&]() {
            context->bus->processDataReadWrite(error);
        });
        context->running = true;
    });
}

dw_ec_result DW_EC_BRIDGE_CALL dw_ec_exchange(dw_ec_context* context,
    const uint8_t* outputs,
    size_t output_size,
    uint8_t* inputs,
    size_t input_size,
    dw_ec_exchange_status* status) {
    if (context == nullptr || status == nullptr || status->struct_size < sizeof(dw_ec_exchange_status)) return DW_EC_INVALID_ARGUMENT;
    if (!context->running) return DW_EC_NOT_READY;
    if (output_size != context->output_size || input_size != context->input_size ||
        (output_size != 0 && outputs == nullptr) || (input_size != 0 && inputs == nullptr)) return DW_EC_INVALID_ARGUMENT;
    return guard(context, [&]() {
        const auto started = std::chrono::steady_clock::now();
        if (output_size != 0) std::memcpy(context->iomap.data() + context->input_size, outputs, output_size);
        bool failed = false;
        context->bus->processDataReadWrite([&](kickcat::DatagramState const&) { failed = true; });
        if (input_size != 0) std::memcpy(inputs, context->iomap.data(), input_size);
        ++context->cycle_count;
        status->expected_wkc = context->expected_wkc;
        status->actual_wkc = failed ? 0 : context->expected_wkc;
        status->exchange_duration_ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count());
        status->cycle_count = context->cycle_count;
        if (failed) throw std::runtime_error("KickCAT reported a lost or invalid process-data datagram.");
    });
}

void DW_EC_BRIDGE_CALL dw_ec_stop(dw_ec_context* context) {
    if (context == nullptr) return;
    context->running = false;
    if (context->bus) {
        try { context->bus->requestState(kickcat::State::SAFE_OP); } catch (...) {}
    }
}

const char* DW_EC_BRIDGE_CALL dw_ec_last_error(dw_ec_context* context) {
    return context == nullptr ? "No EtherCAT bridge context." : context->error.c_str();
}
} // extern C
