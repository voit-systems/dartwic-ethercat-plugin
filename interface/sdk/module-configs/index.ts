import type { ModuleConfigDefinition } from "./types.ts";

/** Defines a module instance configuration component. @dartwic-reference @category Module Configuration */
export function defineModuleConfig(definition: ModuleConfigDefinition): ModuleConfigDefinition {
    return definition;
}

export type { ModuleConfigDefinition, ModuleConfigProps } from "./types.ts";
