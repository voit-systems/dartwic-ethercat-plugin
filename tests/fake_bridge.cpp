#include "dartwic_ethercat_bridge.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>

struct dw_ec_context {
    std::string error;
    uint64_t cycles = 0;
    bool running = false;
};

namespace {
dw_ec_result writeText(const std::string& text, char* destination, size_t size, size_t* required) {
    if (required == nullptr) return DW_EC_INVALID_ARGUMENT;
    *required = text.size() + 1;
    if (destination == nullptr || size == 0) return DW_EC_OK;
    if (size < *required) return DW_EC_INVALID_ARGUMENT;
    std::memcpy(destination, text.c_str(), *required);
    return DW_EC_OK;
}
}

extern "C" {
uint32_t DW_EC_BRIDGE_CALL dw_ec_bridge_abi_version(void) { return DW_EC_BRIDGE_ABI_VERSION; }
dw_ec_result DW_EC_BRIDGE_CALL dw_ec_list_adapters_json(char* out, size_t size, size_t* required) {
    return writeText(R"([{"id":"fake","name":"Fake bridge","kind":"simulator"}])", out, size, required);
}
dw_ec_result DW_EC_BRIDGE_CALL dw_ec_open(const char*, dw_ec_context** context) {
    if (context == nullptr) return DW_EC_INVALID_ARGUMENT;
    *context = new dw_ec_context;
    return DW_EC_OK;
}
void DW_EC_BRIDGE_CALL dw_ec_close(dw_ec_context* context) { delete context; }
dw_ec_result DW_EC_BRIDGE_CALL dw_ec_scan(dw_ec_context*) { return DW_EC_OK; }
dw_ec_result DW_EC_BRIDGE_CALL dw_ec_topology_json(dw_ec_context*, char* out, size_t size, size_t* required) {
    return writeText(R"({"process_image":{"outputs_bytes":16,"inputs_bytes":16},"slaves":[{"position":0,"name":"Fake I/O","outputs":[],"inputs":[]}]})", out, size, required);
}
dw_ec_result DW_EC_BRIDGE_CALL dw_ec_process_image_sizes(dw_ec_context*, size_t* outputs, size_t* inputs) {
    if (outputs == nullptr || inputs == nullptr) return DW_EC_INVALID_ARGUMENT;
    *outputs = 16; *inputs = 16; return DW_EC_OK;
}
dw_ec_result DW_EC_BRIDGE_CALL dw_ec_start(dw_ec_context* context) {
    if (context == nullptr) return DW_EC_INVALID_ARGUMENT;
    context->running = true; return DW_EC_OK;
}
dw_ec_result DW_EC_BRIDGE_CALL dw_ec_exchange(dw_ec_context* context, const uint8_t* outputs,
    size_t output_size, uint8_t* inputs, size_t input_size, dw_ec_exchange_status* status) {
    if (context == nullptr || !context->running) return DW_EC_NOT_READY;
    if (output_size != 16 || input_size != 16 || status == nullptr) return DW_EC_INVALID_ARGUMENT;
    const auto started = std::chrono::steady_clock::now();
    std::copy(outputs, outputs + 16, inputs);
    ++context->cycles;
    status->expected_wkc = 3; status->actual_wkc = 3; status->cycle_count = context->cycles;
    status->exchange_duration_ns = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started).count());
    return DW_EC_OK;
}
void DW_EC_BRIDGE_CALL dw_ec_stop(dw_ec_context* context) { if (context) context->running = false; }
const char* DW_EC_BRIDGE_CALL dw_ec_last_error(dw_ec_context* context) { return context ? context->error.c_str() : "no context"; }
}
