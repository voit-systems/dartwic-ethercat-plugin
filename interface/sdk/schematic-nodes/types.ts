/** Props passed to the canvas renderer for one schematic node. @dartwic-reference @category Schematic Nodes */
export interface SchematicNodeProps {
    data?: any;
    selected?: boolean;
    id?: string;
}

/** Props passed to a schematic node configuration component. @dartwic-reference @category Schematic Nodes */
export interface SchematicNodeConfigProps {
    data?: any;
    updateNode?: (nextData: any) => void;
    defaults?: any;
}

/** Operator command exposed by a schematic node. @dartwic-reference @category Schematic Nodes */
export interface SchematicCommandAction {
    id: string;
    label: string;
    disabled?: boolean;
    hidden?: boolean;
    description?: string;
    input?: {
        type?: "text" | "number";
        placeholder?: string;
        defaultValue?: unknown;
        min?: string | number;
        max?: string | number;
        step?: string | number;
    };
    choice?: {
        items: Array<{value: string; label: string; keywords?: string[]}>;
        defaultValue?: unknown;
        placeholder?: string;
        searchPlaceholder?: string;
        emptyPlaceholder?: string;
    };
    onRun?: (value?: unknown) => unknown | Promise<unknown>;
}

/** Runtime context supplied while building node command actions. @dartwic-reference @category Schematic Nodes */
export interface SchematicCommandContext {
    data?: any;
    id?: string;
    type?: string;
    channels?: Record<string, any>;
    controlEnabled?: boolean;
    operation?: (operationName: string, payload?: any, timeoutMs?: number) => Promise<any>;
}

/** Palette label and default node data for a schematic contribution. @dartwic-reference @category Schematic Nodes */
export interface SchematicNodePaletteDefinition {
    name?: string;
    defaults?: Record<string, unknown>;
}

/** Complete renderer, editor, palette, sizing, and command contract for a node type. @dartwic-reference @category Schematic Nodes */
export interface SchematicNodeDefinition {
    type: string;
    component: (props: SchematicNodeProps) => any;
    configComponent?: (props: SchematicNodeConfigProps) => any;
    getCommandActions?: (context: SchematicCommandContext) => SchematicCommandAction[];
    selectionIcon?: any;
    propDefaults?: Record<string, unknown>;
    dataDefaults?: Record<string, unknown>;
    resizable?: boolean;
    connectable?: boolean;
    useMeasuredSize?: boolean;
    palette?: SchematicNodePaletteDefinition;
}
