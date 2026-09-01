#include "ethercat_codec.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace EtherCAT {
namespace {
    uint64_t readBits(std::span<const uint8_t> image, size_t bit_offset, size_t bit_length) {
        if (bit_length == 0 || bit_length > 64 || bit_offset + bit_length > image.size() * 8) {
            throw std::out_of_range("PDO field is outside the process image.");
        }
        uint64_t value = 0;
        for (size_t bit = 0; bit < bit_length; ++bit) {
            const size_t source = bit_offset + bit;
            value |= static_cast<uint64_t>((image[source / 8] >> (source % 8)) & 1U) << bit;
        }
        return value;
    }

    void writeBits(std::span<uint8_t> image, size_t bit_offset, size_t bit_length, uint64_t value) {
        if (bit_length == 0 || bit_length > 64 || bit_offset + bit_length > image.size() * 8) {
            throw std::out_of_range("PDO field is outside the process image.");
        }
        for (size_t bit = 0; bit < bit_length; ++bit) {
            const size_t destination = bit_offset + bit;
            const uint8_t mask = static_cast<uint8_t>(1U << (destination % 8));
            if ((value >> bit) & 1U) image[destination / 8] |= mask;
            else image[destination / 8] &= static_cast<uint8_t>(~mask);
        }
    }

    int64_t signExtend(uint64_t value, size_t bits) {
        if (bits == 64) return std::bit_cast<int64_t>(value);
        const uint64_t sign = uint64_t{1} << (bits - 1);
        return static_cast<int64_t>((value ^ sign) - sign);
    }

    uint64_t unsignedMaximum(size_t bits) {
        return bits == 64 ? (std::numeric_limits<uint64_t>::max)() : ((uint64_t{1} << bits) - 1);
    }
}

ValueType parseValueType(const std::string& value) {
    if (value == "bool" || value == "boolean") return ValueType::Boolean;
    if (value == "int8") return ValueType::Int8;
    if (value == "uint8") return ValueType::UInt8;
    if (value == "int16") return ValueType::Int16;
    if (value == "uint16") return ValueType::UInt16;
    if (value == "int32") return ValueType::Int32;
    if (value == "uint32") return ValueType::UInt32;
    if (value == "int64") return ValueType::Int64;
    if (value == "uint64") return ValueType::UInt64;
    if (value == "float32") return ValueType::Float32;
    if (value == "float64") return ValueType::Float64;
    throw std::invalid_argument("Unsupported EtherCAT PDO data type `" + value + "`.");
}

const char* valueTypeName(ValueType type) {
    switch (type) {
        case ValueType::Boolean: return "bool";
        case ValueType::Int8: return "int8";
        case ValueType::UInt8: return "uint8";
        case ValueType::Int16: return "int16";
        case ValueType::UInt16: return "uint16";
        case ValueType::Int32: return "int32";
        case ValueType::UInt32: return "uint32";
        case ValueType::Int64: return "int64";
        case ValueType::UInt64: return "uint64";
        case ValueType::Float32: return "float32";
        case ValueType::Float64: return "float64";
    }
    return "unknown";
}

double decodeValue(std::span<const uint8_t> image, size_t bit_offset, size_t bit_length,
    ValueType type, double scale, double offset) {
    const uint64_t raw = readBits(image, bit_offset, bit_length);
    double numeric = 0.0;
    switch (type) {
        case ValueType::Boolean: numeric = raw == 0 ? 0.0 : 1.0; break;
        case ValueType::Int8:
        case ValueType::Int16:
        case ValueType::Int32:
        case ValueType::Int64: numeric = static_cast<double>(signExtend(raw, bit_length)); break;
        case ValueType::UInt8:
        case ValueType::UInt16:
        case ValueType::UInt32:
        case ValueType::UInt64: numeric = static_cast<double>(raw); break;
        case ValueType::Float32:
            if (bit_length != 32) throw std::invalid_argument("float32 PDO entries must be 32 bits.");
            numeric = static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(raw)));
            break;
        case ValueType::Float64:
            if (bit_length != 64) throw std::invalid_argument("float64 PDO entries must be 64 bits.");
            numeric = std::bit_cast<double>(raw);
            break;
    }
    return numeric * scale + offset;
}

void encodeValue(std::span<uint8_t> image, size_t bit_offset, size_t bit_length,
    ValueType type, double value, double scale, double offset) {
    if (!std::isfinite(value) || !std::isfinite(scale) || scale == 0.0 || !std::isfinite(offset)) {
        throw std::invalid_argument("PDO values, scale, and offset must be finite and scale must be non-zero.");
    }
    const double converted = (value - offset) / scale;
    uint64_t raw = 0;
    switch (type) {
        case ValueType::Boolean: raw = converted != 0.0 ? 1 : 0; break;
        case ValueType::Int8:
        case ValueType::Int16:
        case ValueType::Int32:
        case ValueType::Int64: {
            const long double minimum = bit_length == 64
                ? static_cast<long double>((std::numeric_limits<int64_t>::min)())
                : -static_cast<long double>(uint64_t{1} << (bit_length - 1));
            const long double maximum = bit_length == 64
                ? static_cast<long double>((std::numeric_limits<int64_t>::max)())
                : static_cast<long double>((uint64_t{1} << (bit_length - 1)) - 1);
            const auto rounded = std::clamp(static_cast<long double>(std::llround(converted)), minimum, maximum);
            raw = static_cast<uint64_t>(static_cast<int64_t>(rounded));
            break;
        }
        case ValueType::UInt8:
        case ValueType::UInt16:
        case ValueType::UInt32:
        case ValueType::UInt64: {
            const long double rounded = std::round(static_cast<long double>(converted));
            raw = static_cast<uint64_t>(std::clamp(rounded, 0.0L,
                static_cast<long double>(unsignedMaximum(bit_length))));
            break;
        }
        case ValueType::Float32:
            if (bit_length != 32) throw std::invalid_argument("float32 PDO entries must be 32 bits.");
            raw = std::bit_cast<uint32_t>(static_cast<float>(converted));
            break;
        case ValueType::Float64:
            if (bit_length != 64) throw std::invalid_argument("float64 PDO entries must be 64 bits.");
            raw = std::bit_cast<uint64_t>(converted);
            break;
    }
    writeBits(image, bit_offset, bit_length, raw);
}
}
