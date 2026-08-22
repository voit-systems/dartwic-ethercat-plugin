import type { ModuleConfigDefinition } from "../module-configs/types.ts";
import type { PluginSettingsDefinition } from "../plugin-settings/types.ts";
import type { ResourceDefinition } from "../resources/types.ts";
import type { SchematicNodeDefinition } from "../schematic-nodes/types.ts";
import type { TaskCardDefinition, TaskConfigDefinition } from "../tasks/types.ts";

/** Shared local identity and display name for plugin contributions. @dartwic-reference @category Plugin Registration */
export interface NamedContribution {
    id: string;
    name: string;
}

/** Registers task card and editor components under one task type. @dartwic-reference @category Tasks */
export interface TaskUiDefinition extends NamedContribution {
    card?: TaskCardDefinition["component"];
    editor?: TaskConfigDefinition["component"];
}

/** Registers icon and panel components for a module type. @dartwic-reference @category Module Configuration */
export interface ModuleUiDefinition extends NamedContribution {
    icon?: (props: {className?: string; size?: number; "aria-hidden"?: boolean}) => any;
    panel?: ModuleConfigDefinition["component"];
}

/** Registers a resource type through the plugin registrar. @dartwic-reference @category Resources */
export interface ResourceContributionDefinition extends ResourceDefinition {
    id: string;
    name: string;
}

/** Registers a schematic node under a plugin-qualified type. @dartwic-reference @category Schematic Nodes */
export interface SchematicNodeContributionDefinition extends Omit<SchematicNodeDefinition, "type">, NamedContribution {}

/** Registers a plugin settings panel. @dartwic-reference @category Plugin Settings */
export interface SettingsPanelDefinition extends NamedContribution {
    component: PluginSettingsDefinition["component"];
}

/** Focused registry supplied to an interface plugin's `register` callback. @dartwic-reference @category Plugin Registration */
export interface InterfacePluginRegistrar {
    addTaskUi(definition: TaskUiDefinition): string;
    addModuleUi(definition: ModuleUiDefinition): string;
    addResource(definition: ResourceContributionDefinition): string;
    addSchematicNode(definition: SchematicNodeContributionDefinition): string;
    addSettingsPanel(definition: SettingsPanelDefinition): string;
}

/** Public definition consumed by `definePlugin`. @dartwic-reference @category Plugin Registration */
export interface InterfacePluginDefinition {
    id: string;
    name?: string;
    register(registry: InterfacePluginRegistrar): void;
}

/** Normalized contribution entry returned for inspection and host integration. @dartwic-reference @category Plugin Registration */
export interface InterfaceContributionEntry extends NamedContribution {
    [key: string]: unknown;
}

/** Contributions grouped by supported interface extension point. @dartwic-reference @category Plugin Registration */
export interface InterfaceContributions {
    taskUis: InterfaceContributionEntry[];
    moduleUis: InterfaceContributionEntry[];
    resources: InterfaceContributionEntry[];
    schematicNodes: InterfaceContributionEntry[];
    settingsPanels: InterfaceContributionEntry[];
}

/** Runtime form of an interface plugin after registration has been evaluated. @dartwic-reference @category Plugin Registration */
export interface MaterializedInterfacePlugin {
    id: string;
    name: string;
    taskTypes: string[];
    taskCards: Record<string, TaskCardDefinition>;
    taskEditors: Record<string, TaskConfigDefinition>;
    moduleUis: Record<string, ModuleUiDefinition>;
    resources: ResourceDefinition[];
    schematicNodes: SchematicNodeDefinition[];
    settingsPanels: SettingsPanelDefinition[];
    contributions: {interface: InterfaceContributions};
}

/** Host services and shared component inventory supplied to a loaded interface plugin. @dartwic-reference @category Runtime */
export interface InterfacePluginHostApi {
    React: Record<string, unknown>;
    useDartwic: (...args: unknown[]) => unknown;
    components: Record<string, unknown>;
    helpers: Record<string, unknown>;
    sdk: {
        styling: {
            cn: (...inputs: unknown[]) => string;
        };
    };
}
