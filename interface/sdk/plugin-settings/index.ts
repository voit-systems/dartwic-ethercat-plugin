import type { PluginSettingsDefinition } from "./types.ts";

/** Defines a plugin settings component. @dartwic-reference @category Plugin Settings */
export function definePluginSettings(definition: PluginSettingsDefinition): PluginSettingsDefinition {
    return definition;
}

export type { PluginSettingsDefinition, PluginSettingsProps } from "./types.ts";
