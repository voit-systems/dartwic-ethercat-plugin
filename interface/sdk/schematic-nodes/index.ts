import type { SchematicNodeDefinition } from "./types.ts";

/** Defines a schematic node renderer and its editing behavior. @dartwic-reference @category Schematic Nodes */
export function defineSchematicNode(definition: SchematicNodeDefinition): SchematicNodeDefinition {
    return definition;
}

export type {
    SchematicCommandAction,
    SchematicCommandContext,
    SchematicNodeConfigProps,
    SchematicNodeDefinition,
    SchematicNodePaletteDefinition,
    SchematicNodeProps
} from "./types.ts";
