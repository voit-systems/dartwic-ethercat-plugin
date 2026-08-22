# DARTWIC 2.0 Example Plugin

This is the canonical plugin starter and a complete combined engine/interface example.

Developers edit `plugin.json`, `engine/include`, `engine/src`, `interface/src`, and optional `files/`. Everything under `engine/include/sdk` and `interface/sdk` is synchronized DARTWIC SDK content.

Engine registration happens in `ExampleDevicePlugin::onPluginLoaded()` through `dartwic->registerModuleType`, `registerShareType`, `registerTaskType`, `registerOperation`, `registerDCodeFunction`, and `registerLoop`. IDs are local and become `<plugin-id>.<local-id>`. `createModule` receives the local module ID. The `example_flight_link` Share Type implements `DARTWIC::API::ShareTransport`: it moves complete DARTWIC Share frames and never registers separate RAPID, ARGUS, command, or bulk handlers. Its registration includes UI-editable default connection values. With `receive_endpoint` and `send_endpoint` configured it uses raw ZeroMQ without TEMPEST; without those fields it runs as the deterministic simulated transport used by the focused plugin test.

The interface uses `definePlugin({register})`. `addModuleUi` demonstrates both a React icon and a module configuration panel. The module config falls back to `workspace/global_data/plugin_icons/example-device.svg`, which is supplied through `files/`.

Place every plugin-authored engine-root file under `files/`. This example includes a schematic-node JSON file at `files/workspace/global_data/schematic_nodes/device_examples/example_indicator.json`; packaging includes that tree and installation or local deployment mirrors it below the engine configuration root.

`npm run package` creates the release `plugin.zip` with `engine/`, `interface/`, and optional `files/`. `npm run package-debug` creates `plugin-debug.zip` with `engine-debug/`, `interface/`, and optional `files/`. Release assets are `plugin.json` and the matching archive; there is no `contributions.json` or extra version manifest.

Commands:

- `npm run build` bundles the interface plugin.
- `npm run sync-sdk` refreshes this standalone example's bundled engine and interface SDK snapshots.
- `npm run verify:sdk` verifies bundled SDK snapshots.
- `npm run verify` validates packaged manifests and materializes the live interface registry.
- `npm run package` builds, verifies, and creates the release `plugin.zip`.
- `npm run package-debug` builds, verifies, and creates `plugin-debug.zip`.
- `npm run deploy` builds the release engine plugin and interface plugin, then copies both sides to configured local targets.
- `npm run deploy-debug` does the same with the debug engine plugin.
