import React from "../sdk/react.ts";

export function ExampleResource({setIsLoaded}) {
    React.useEffect(() => {
        setIsLoaded?.(true);
    }, [setIsLoaded]);

    return (
        <div className="p-4 text-sm text-muted-foreground">
            This resource page was registered by the example plugin.
        </div>
    );
}

export function ExampleSchematicNode({data}) {
    return (
        <div className="h-full w-full rounded-md border border-border bg-card p-2 text-xs text-foreground">
            {data?.label ?? "Example"}
        </div>
    );
}
