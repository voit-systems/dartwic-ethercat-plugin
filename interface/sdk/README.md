# Interface Plugin SDK

The DARTWIC 2.0 interface SDK has one authoring pattern: define a plugin and register each contribution through the registry passed to `register`.

```jsx
import {definePlugin} from "@dartwic/interface-sdk";

export default definePlugin({
  id: "my_plugin",
  name: "My Plugin",
  register(registry) {
    registry.addTaskUi({id: "read", name: "Read Task", card: ReadCard, editor: ReadEditor});
    registry.addModuleUi({id: "device", name: "Device", icon: DeviceIcon, panel: DevicePanel});
    registry.addResource({id: "reports", name: "Reports", type: "component", component: Reports});
    registry.addSchematicNode({
      id: "gauge",
      name: "Gauge",
      component: Gauge,
      dataDefaults: {label: "Gauge"},
      palette: {defaults: {width: 160, height: 80}},
    });
    registry.addSettingsPanel({id: "general", name: "Settings", component: Settings});
  },
});
```

IDs passed to the registry are local. The host validates them and exposes them as `<plugin-id>.<local-id>`. Duplicate IDs in a category fail plugin loading instead of replacing another registration.

The runtime entry is deliberately small:

```js
import {registerPlugin} from "@dartwic/interface-sdk/runtime";
import plugin from "./plugin.jsx";

registerPlugin(plugin);
```

Build output is `ui/index.js`. The live registry supplies installed-plugin contribution counts; packages contain no generated contribution metadata.

Schematic node renderers and configuration UI are interface-owned. Place engine-readable palette JSON beneath `files/workspace/global_data/schematic_nodes/...`; installation mirrors it into the engine configuration root.

Public entrypoints include the main registry types, `tasks`, `resources`, `plugin-settings`, `module-configs`, `schematic-nodes`, `hooks`, `ui`, `utils`, `runtime`, `react`, and the Tailwind preset.
