# DARTWIC EtherCAT Plugin

High-speed cyclic EtherCAT I/O for DARTWIC 2.0. The plugin runs one synchronous, full-duplex process-data exchange per periodic task tick and is designed for a 1 ms (1 kHz) task period.

The repository is standalone and does not contain or require the private DARTWIC source tree. Versioned Engine and Interface SDK snapshots are bundled for plugin development.

## Architecture

The runtime is intentionally split at a C ABI boundary:

```text
DARTWIC periodic task (ClangCL / Clang)
  ├─ batch snapshot fixed command channels
  ├─ encode the complete output process image
  ├─ call one synchronous C bridge exchange
  ├─ decode the complete input process image
  └─ batch-stage fixed telemetry channels
                    │
                    ▼
dartwic_ethercat_bridge.dll / .so (GCC or MinGW GCC)
                    │
                    ▼
        KickCAT master + NIC or simulator
```

No C++ objects cross the compiler boundary. The public bridge interface is `engine/include/dartwic_ethercat_bridge.h`, and `DW_EC_BRIDGE_ABI_VERSION` protects against incompatible binaries.

One `ethercat.cycle` task exclusively owns one `ethercat.master` module while running. Reads and writes occur in the same EtherCAT frame cycle. There are no per-channel network operations, background command queues, or change-detection delays in the hot path.

## Configuration

1. Create an **EtherCAT Master** module.
2. Select **KickCAT Simulator** or **Physical EtherCAT Adapter** from the dropdown.
3. Create an **EtherCAT Cyclic I/O** periodic task and set its period to `1 ms` for 1 kHz operation.
4. Select the master and click **Scan Bus**.
5. Add mappings using the discovered slave/PDO dropdown, then select the corresponding RAPID channel.

The first release uses the PDO layout already exposed by each slave. Dynamic PDO reassignment is deliberately out of scope. Stopping a task stops cyclic exchange; it does not overwrite the last process-output values with an assumed safe state.

The built-in simulator exposes:

- Outputs: `Command U32`, `Command F64`, and `Command Bool`.
- Inputs: matching echo values plus `Cycle Counter`.

It uses KickCAT's real master, EtherCAT state machine, process-image mapping, emulated ESC, and loopback transport in-process. It is not a mock of the plugin API.

## Build the DARTWIC plugin

Requirements: Node.js 20+, CMake 3.23+, a vcpkg checkout, and the same supported compiler used by DARTWIC (ClangCL on Windows or Clang on Linux).

```shell
npm ci
npm run build:interface
cmake --preset windows-clang-release
cmake --build --preset build-windows-clang-release --target copy_engine_plugin
```

Set `VCPKG_ROOT` before using the presets. On Linux, use `linux-clang-release` and `build-linux-clang-release`.

## Build the KickCAT C bridge

KickCAT is fetched from [`leducp/KickCAT`](https://github.com/leducp/KickCAT) and pinned to commit `f10386d54f734d388a405b4dae506801e35c238b` (`v2.6-rc3`).

Linux:

```shell
cmake -S bridge -B build/bridge -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build/bridge
```

Windows uses a POSIX MinGW-w64 GCC toolchain because KickCAT's Windows backend is built for that environment. KickCAT's tested Windows dependency path is Conan, which supplies the Npcap SDK used by the bridge:

```shell
python -m pip install conan==2.19.1
conan profile detect --force
conan install bridge/conanfile.txt \
  --output-folder=build/bridge-conan \
  --build=missing \
  --settings=build_type=Release

cmake -S bridge -B build/bridge -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/bridge-conan/conan_toolchain.cmake"
cmake --build build/bridge
```

Pass the resulting bridge to the plugin build so it is copied beside the plugin binary:

```shell
cmake --preset windows-clang-release \
  -DDARTWIC_ETHERCAT_BRIDGE_PATH=/absolute/path/to/dartwic_ethercat_bridge.dll
```

Npcap must be installed on the target Windows machine because the Windows bridge contains KickCAT's Npcap backend. The simulator does not require EtherCAT hardware or a dedicated NIC.

## Tests and 1 kHz rate check

The standard native test suite covers unaligned PDO encoding, signed/scaled values, dynamic C bridge loading, ABI validation, full-duplex exchange, and a 20,000-cycle bridge throughput test:

```shell
ctest --test-dir build/windows-clang-release -C Release --output-on-failure
```

`ethercat_rate_test` runs the same synchronous exchange interface used by the plugin and reports mean/p99 exchange time and 1 ms deadline misses:

```shell
ethercat_rate_test --bridge /path/to/dartwic_ethercat_bridge.dll --cycles 10000 --period-us 1000
```

Run it first against the built-in simulator, then against the intended NIC and slave chain. A simulator pass validates software overhead and cadence; it does not prove the host OS, NIC driver, cabling, and physical slaves can sustain the same timing.

The plugin publishes these fixed diagnostic channels per task:

- `<task>.ethercat.exchange_time_us`
- `<task>.ethercat.actual_wkc`
- `<task>.ethercat.expected_wkc`
- `<task>.ethercat.failure_count`

Three consecutive exchange failures stop the task. Automatic reconnect is intentionally deferred so a control system does not silently resume outputs after an ambiguous bus interruption.

## Repository layout

- `engine/`: DARTWIC module, cyclic task, batch channel path, PDO codec, and bridge loader.
- `bridge/`: KickCAT-backed C ABI shared library.
- `interface/`: dropdown-driven module and task editors.
- `tests/`: codec, ABI loader, fake bridge, and throughput tests.
- `tools/ethercat_rate_test.cpp`: hardware-free and physical-bus rate test.

## Current scope

This is an initial implementation intended for simulator validation and hardware bring-up. Before production control use, validate the exact slave chain on the target Linux real-time configuration, define application-specific output safety behavior, and measure worst-case—not only average—cycle latency.
