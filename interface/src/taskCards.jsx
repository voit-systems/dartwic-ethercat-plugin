import React from "../sdk/react.ts";
import {Separator} from "../sdk/ui/general.ts";

export function EthercatTaskCard({task}) {
    const mappings = Array.isArray(task.arguments?.mappings) ? task.arguments.mappings : [];
    return <><Separator/><div className="grid grid-cols-2 gap-2 text-xs">
        <div className="rounded-md border bg-muted/40 px-3 py-2"><div className="text-muted-foreground">MASTER</div><div className="truncate">{task.arguments?.module_instance_name || "UNBOUND"}</div></div>
        <div className="rounded-md border bg-muted/40 px-3 py-2"><div className="text-muted-foreground">PDO CHANNELS</div><div>{mappings.length}</div></div>
    </div></>;
}
