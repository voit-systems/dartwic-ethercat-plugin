#include "ethercat_bridge_loader.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        EtherCAT::BridgeLibrary library{std::filesystem::path(argv[1])};
        if (library.listAdapters().empty()) return 1;
        EtherCAT::BridgeLibrary::Master master(library, {{"adapter", "fake"}});
        const auto topology = master.scan();
        if (topology.at("slaves").size() != 1 || master.outputSize() != 16 || master.inputSize() != 16) return 1;
        master.start();
        std::vector<uint8_t> outputs(16, 0), inputs(16, 0);
        constexpr size_t cycles = 20000;
        const auto started = std::chrono::steady_clock::now();
        for (size_t cycle = 0; cycle < cycles; ++cycle) {
            outputs[0] = static_cast<uint8_t>(cycle);
            const auto status = master.exchange(outputs, inputs);
            if (inputs[0] != outputs[0] || status.actual_wkc != status.expected_wkc) return 1;
        }
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        const double exchanges_per_second = cycles / elapsed;
        std::cout << exchanges_per_second << " bridge exchanges/s\n";
        master.stop();
        return exchanges_per_second >= 1000.0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
