import fs from "node:fs/promises";
import path from "node:path";
import {fileURLToPath} from "node:url";

import {pathExists, repoRoot, runCommand} from "./plugin-utils.mjs";

const conanVersion = "2.32.0";

function prependPath(environment, ...entries) {
  const separator = process.platform === "win32" ? ";" : ":";
  return {
    ...environment,
    PATH: [...entries, environment.PATH ?? ""].filter(Boolean).join(separator),
  };
}

async function requireFile(filePath, description) {
  if (!(await pathExists(filePath))) {
    throw new Error(`${description} was not found at ${filePath}.`);
  }
}

async function ensureWindowsBridge() {
  const msysRoot = path.resolve(process.env.MSYS2_ROOT || "C:/msys64");
  const toolchainBin = path.join(msysRoot, "ucrt64", "bin");
  const msysBin = path.join(msysRoot, "usr", "bin");
  const gcc = path.join(toolchainBin, "gcc.exe");
  const gxx = path.join(toolchainBin, "g++.exe");
  const cmake = path.join(toolchainBin, "cmake.exe");
  const ninja = path.join(toolchainBin, "ninja.exe");
  const python = path.join(toolchainBin, "python.exe");

  for (const [filePath, description] of [
    [gcc, "MSYS2 UCRT64 GCC"],
    [gxx, "MSYS2 UCRT64 G++"],
    [cmake, "MSYS2 UCRT64 CMake"],
    [ninja, "MSYS2 UCRT64 Ninja"],
    [python, "MSYS2 UCRT64 Python"],
  ]) {
    await requireFile(filePath, description);
  }

  const environment = prependPath(process.env, toolchainBin, msysBin);
  const venvDir = path.resolve(repoRoot, "build", "bridge-conan-venv");
  const venvPython = path.join(venvDir, "bin", "python.exe");
  const conan = path.join(venvDir, "bin", "conan.exe");
  if (!(await pathExists(conan))) {
    await fs.mkdir(path.dirname(venvDir), {recursive: true});
    await runCommand(python, ["-m", "venv", venvDir], {env: environment});
    await runCommand(venvPython, ["-m", "pip", "install", `conan==${conanVersion}`], {env: environment});
  }

  const conanOutput = path.resolve(repoRoot, "build", "bridge-conan");
  const mingwProfile = path.resolve(repoRoot, "bridge", "profiles", "mingw-ucrt64");
  await runCommand(conan, [
    "install", path.resolve(repoRoot, "bridge", "conanfile.txt"),
    `--output-folder=${conanOutput}`,
    "--build=missing",
    `--profile:host=${mingwProfile}`,
    `--profile:build=${mingwProfile}`,
  ], {env: environment});

  const buildDir = path.resolve(repoRoot, "build", "bridge-windows");
  await runCommand(cmake, [
    "--fresh",
    "-S", path.resolve(repoRoot, "bridge"),
    "-B", buildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    `-DCMAKE_MAKE_PROGRAM=${ninja}`,
    `-DCMAKE_C_COMPILER=${gcc}`,
    `-DCMAKE_CXX_COMPILER=${gxx}`,
    `-DCMAKE_TOOLCHAIN_FILE=${path.join(conanOutput, "conan_toolchain.cmake")}`,
  ], {env: environment});
  await runCommand(cmake, ["--build", buildDir], {env: environment});

  const bridge = path.join(buildDir, "dartwic_ethercat_bridge.dll");
  await requireFile(bridge, "Built KickCAT bridge");
  return bridge;
}

async function ensureUnixBridge() {
  const buildDir = path.resolve(repoRoot, "build", "bridge");
  const configureArguments = [
    "-S", path.resolve(repoRoot, "bridge"),
    "-B", buildDir,
    "-DCMAKE_BUILD_TYPE=Release",
  ];
  const vcpkgRoot = String(process.env.VCPKG_ROOT ?? "").trim();
  const toolchain = vcpkgRoot
    ? path.resolve(vcpkgRoot, "scripts", "buildsystems", "vcpkg.cmake")
    : "";
  if (toolchain && await pathExists(toolchain)) {
    configureArguments.push(`-DCMAKE_TOOLCHAIN_FILE=${toolchain}`);
  }
  await runCommand("cmake", configureArguments);
  await runCommand("cmake", ["--build", buildDir, "--parallel"]);
  const bridgeName = process.platform === "darwin"
    ? "libdartwic_ethercat_bridge.dylib"
    : "libdartwic_ethercat_bridge.so";
  const bridge = path.join(buildDir, bridgeName);
  await requireFile(bridge, "Built KickCAT bridge");
  return bridge;
}

export async function ensureBridge() {
  process.stdout.write("Building the KickCAT C bridge.\n");
  const bridge = process.platform === "win32"
    ? await ensureWindowsBridge()
    : await ensureUnixBridge();
  process.stdout.write(`KickCAT C bridge ready at ${bridge}.\n`);
  return bridge;
}

const invokedPath = process.argv[1] ? path.resolve(process.argv[1]) : "";
if (invokedPath === fileURLToPath(import.meta.url)) {
  await ensureBridge();
}
