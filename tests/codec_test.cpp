#include "ethercat_codec.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
bool near(double left, double right) { return std::abs(left - right) < 1e-6; }
}

int main() {
    std::vector<uint8_t> image(32, 0);
    EtherCAT::encodeValue(image, 3, 1, EtherCAT::ValueType::Boolean, 1.0, 1.0, 0.0);
    EtherCAT::encodeValue(image, 9, 16, EtherCAT::ValueType::Int16, -1234.0, 1.0, 0.0);
    EtherCAT::encodeValue(image, 32, 32, EtherCAT::ValueType::Float32, 12.5, 1.0, 0.0);
    EtherCAT::encodeValue(image, 64, 64, EtherCAT::ValueType::Float64, 9.75, 2.0, 1.0);
    if (!near(EtherCAT::decodeValue(image, 3, 1, EtherCAT::ValueType::Boolean, 1.0, 0.0), 1.0) ||
        !near(EtherCAT::decodeValue(image, 9, 16, EtherCAT::ValueType::Int16, 1.0, 0.0), -1234.0) ||
        !near(EtherCAT::decodeValue(image, 32, 32, EtherCAT::ValueType::Float32, 1.0, 0.0), 12.5) ||
        !near(EtherCAT::decodeValue(image, 64, 64, EtherCAT::ValueType::Float64, 2.0, 1.0), 9.75)) {
        std::cerr << "EtherCAT PDO codec round-trip failed.\n";
        return 1;
    }
    return 0;
}
