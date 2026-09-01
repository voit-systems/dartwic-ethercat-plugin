#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace EtherCAT {
    enum class ValueType {
        Boolean,
        Int8,
        UInt8,
        Int16,
        UInt16,
        Int32,
        UInt32,
        Int64,
        UInt64,
        Float32,
        Float64
    };

    ValueType parseValueType(const std::string& value);
    const char* valueTypeName(ValueType type);
    double decodeValue(std::span<const uint8_t> image, size_t bit_offset, size_t bit_length,
        ValueType type, double scale, double offset);
    void encodeValue(std::span<uint8_t> image, size_t bit_offset, size_t bit_length,
        ValueType type, double value, double scale, double offset);
}
