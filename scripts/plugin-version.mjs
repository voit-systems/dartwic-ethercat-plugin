import {
  packageJsonPath,
  pluginManifestPath,
  readJson,
  validatePluginManifest,
  writeJson,
} from "./plugin-utils.mjs";

function shouldValidateOnly(argv) {
  return argv.includes("--validate");
}

async function validateOnly() {
  const packageJson = await readJson(packageJsonPath);
  const pluginManifest = await readJson(pluginManifestPath);

  if (!packageJson || typeof packageJson !== "object") {
    throw new Error("package.json must contain an object.");
  }

  validatePluginManifest(pluginManifest);

  const packageVersion = String(packageJson.version ?? "").trim();
  if (!packageVersion) {
    throw new Error("package.json version is required.");
  }
}

async function syncPluginVersion() {
  const packageJson = await readJson(packageJsonPath);
  const pluginManifest = await readJson(pluginManifestPath);

  if (!packageJson || typeof packageJson !== "object") {
    throw new Error("package.json must contain an object.");
  }

  validatePluginManifest(pluginManifest, { requireVersion: false });

  const packageVersion = String(packageJson.version ?? "").trim();
  if (!packageVersion) {
    throw new Error("package.json version is required.");
  }

  pluginManifest.version = packageVersion;
  await writeJson(pluginManifestPath, pluginManifest);

  process.stdout.write(`Synchronized plugin.json for version ${packageVersion}.\n`);
}

if (shouldValidateOnly(process.argv.slice(2))) {
  await validateOnly();
} else {
  await syncPluginVersion();
}
