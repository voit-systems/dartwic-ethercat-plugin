#ifndef DARTWIC_ETHERCAT_BRIDGE_H
#define DARTWIC_ETHERCAT_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define DW_EC_BRIDGE_EXPORT __declspec(dllexport)
#define DW_EC_BRIDGE_CALL __cdecl
#else
#define DW_EC_BRIDGE_EXPORT __attribute__((visibility("default")))
#define DW_EC_BRIDGE_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum { DW_EC_BRIDGE_ABI_VERSION = 1 };

typedef struct dw_ec_context dw_ec_context;

typedef enum dw_ec_result {
    DW_EC_OK = 0,
    DW_EC_INVALID_ARGUMENT = 1,
    DW_EC_NOT_READY = 2,
    DW_EC_IO_ERROR = 3,
    DW_EC_TOPOLOGY_ERROR = 4,
    DW_EC_INTERNAL_ERROR = 5
} dw_ec_result;

typedef struct dw_ec_exchange_status {
    uint32_t struct_size;
    uint32_t expected_wkc;
    uint32_t actual_wkc;
    uint32_t reserved;
    uint64_t exchange_duration_ns;
    uint64_t cycle_count;
} dw_ec_exchange_status;

DW_EC_BRIDGE_EXPORT uint32_t DW_EC_BRIDGE_CALL dw_ec_bridge_abi_version(void);
DW_EC_BRIDGE_EXPORT dw_ec_result DW_EC_BRIDGE_CALL dw_ec_list_adapters_json(
    char* destination, size_t destination_size, size_t* required_size);
DW_EC_BRIDGE_EXPORT dw_ec_result DW_EC_BRIDGE_CALL dw_ec_open(
    const char* config_json, dw_ec_context** context);
DW_EC_BRIDGE_EXPORT void DW_EC_BRIDGE_CALL dw_ec_close(dw_ec_context* context);
DW_EC_BRIDGE_EXPORT dw_ec_result DW_EC_BRIDGE_CALL dw_ec_scan(dw_ec_context* context);
DW_EC_BRIDGE_EXPORT dw_ec_result DW_EC_BRIDGE_CALL dw_ec_topology_json(
    dw_ec_context* context, char* destination, size_t destination_size, size_t* required_size);
DW_EC_BRIDGE_EXPORT dw_ec_result DW_EC_BRIDGE_CALL dw_ec_process_image_sizes(
    dw_ec_context* context, size_t* output_size, size_t* input_size);
DW_EC_BRIDGE_EXPORT dw_ec_result DW_EC_BRIDGE_CALL dw_ec_start(dw_ec_context* context);
DW_EC_BRIDGE_EXPORT dw_ec_result DW_EC_BRIDGE_CALL dw_ec_exchange(
    dw_ec_context* context,
    const uint8_t* outputs,
    size_t output_size,
    uint8_t* inputs,
    size_t input_size,
    dw_ec_exchange_status* status);
DW_EC_BRIDGE_EXPORT void DW_EC_BRIDGE_CALL dw_ec_stop(dw_ec_context* context);
DW_EC_BRIDGE_EXPORT const char* DW_EC_BRIDGE_CALL dw_ec_last_error(dw_ec_context* context);

#ifdef __cplusplus
}
#endif

#endif
