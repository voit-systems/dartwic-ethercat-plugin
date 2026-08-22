import fs from "node:fs/promises";
import path from "node:path";
import vm from "node:vm";

import {verifySdkLock} from "./plugin-sdk-lock.mjs";
import {
  getPluginSidePaths,
  pluginManifestPath,
  readJson,
  validatePluginManifest,
} from "./plugin-utils.mjs";

async function materializeInterfacePlugin(pluginId, runtimePath) {
  const sandbox = {window: {}, console};
  sandbox.globalThis = sandbox;
  vm.createContext(sandbox);
  vm.runInContext(await fs.readFile(runtimePath, "utf8"), sandbox, {filename: runtimePath});
  const registration = sandbox.window.__dartwicPluginRegistry__?.[pluginId];
  if (!registration || typeof registration.createPlugin !== "function") {
    throw new Error(`Packaged interface runtime did not register '${pluginId}'.`);
  }
  return registration.createPlugin({React: {}});
}

async function main() {
  await verifySdkLock();
  const manifest = await readJson(pluginManifestPath);
  const {pluginId} = validatePluginManifest(manifest);
  const {engineReleaseDir, interfaceDir} = getPluginSidePaths(pluginId);

  if (manifest.contains_engine_plugin) {
    validatePluginManifest(await readJson(path.resolve(engineReleaseDir, "plugin.json")));
  }

  if (manifest.contains_interface_plugin) {
    const runtime = await materializeInterfacePlugin(pluginId, path.resolve(interfaceDir, "ui", "index.js"));
    if (!runtime?.contributions?.interface || !runtime?.moduleUis?.[`${pluginId}.example_device`]) {
      throw new Error("Packaged interface runtime did not materialize the expected live registry.");
    }
  }

  process.stdout.write(`Verified packaged plugin '${pluginId}' and its live interface registry.\n`);
}

await main();
