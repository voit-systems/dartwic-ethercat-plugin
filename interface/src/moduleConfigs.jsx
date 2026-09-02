import React from "../sdk/react.ts";
import {Button, Input, Label, Select, SelectContent, SelectItem, SelectTrigger, SelectValue} from "../sdk/ui/general.ts";

function unwrap(result) {
    if (result?.error) throw new Error(result?.payload?.error || "EtherCAT operation failed.");
    return result?.payload ?? result ?? [];
}

export function EthercatModuleConfig({instanceConfig, setInstanceConfig, save, operation, moduleEditor}) {
    const parameters = instanceConfig?.parameters || {};
    const [adapters, setAdapters] = React.useState([]);
    const [loading, setLoading] = React.useState(false);
    const [error, setError] = React.useState("");
    const [saved, setSaved] = React.useState(parameters);
    const isDirty = JSON.stringify(parameters) !== JSON.stringify(saved);
    function update(key, value) {
        setInstanceConfig((current) => ({...current, parameters: {...(current?.parameters || {}), [key]: value}}));
    }
    const loadAdapters = React.useCallback(async () => {
        setLoading(true); setError("");
        try {
            const value = unwrap(await operation("ethercat.adapters", {}, 15000));
            setAdapters(Array.isArray(value) ? value : []);
        } catch (reason) { setError(reason instanceof Error ? reason.message : String(reason)); }
        finally { setLoading(false); }
    }, [operation]);
    React.useEffect(() => { void loadAdapters(); }, [loadAdapters]);
    React.useEffect(() => {
        moduleEditor?.register?.({isDirty, isSaving: false,
            canSave: Boolean(parameters.adapter), errorMessage: error,
            saveLabel: "SAVE CONFIG", onSave: async () => { await save(); setSaved(parameters); }});
    }, [moduleEditor, isDirty, parameters, error, save]);
    const adapterOptions = React.useMemo(() => {
        const options = adapters.filter((adapter) => adapter.kind === "hardware");
        if (parameters.adapter && !options.some((adapter) => adapter.id === parameters.adapter)) {
            options.push({id: parameters.adapter, name: `${parameters.adapter} (saved)`, kind: "hardware"});
        }
        return options;
    }, [adapters, parameters.adapter]);
    return <div className="space-y-5">
        <div className="rounded-md border bg-muted/20 p-4">
            <div className="flex items-start justify-between gap-3">
                <div><div className="text-sm font-semibold">PHYSICAL ETHERCAT MASTER</div>
                    <div className="mt-1 text-xs text-muted-foreground">KickCAT exchanges PDO data directly through the selected Ethernet adapter.</div></div>
                <div className="rounded border px-2 py-1 text-[10px] font-semibold text-muted-foreground">{loading ? "DISCOVERING" : `${adapters.length} FOUND`}</div>
            </div>
        </div>
        <div className="space-y-2">
            <div className="flex items-center justify-between"><Label>ETHERCAT NETWORK ADAPTER</Label><Button variant="outline" disabled={loading} onClick={loadAdapters}>{loading ? "REFRESHING…" : "REFRESH ADAPTERS"}</Button></div>
            <Select value={parameters.adapter || ""} onValueChange={(value) => update("adapter", value)}><SelectTrigger><SelectValue placeholder="SELECT A DEDICATED ETHERNET ADAPTER"/></SelectTrigger><SelectContent>
                {adapterOptions.map((adapter) => <SelectItem key={adapter.id} value={adapter.id}>{adapter.name || adapter.id}</SelectItem>)}
            </SelectContent></Select>
            {parameters.adapter ? <div className="break-all text-[11px] text-muted-foreground">ADAPTER ID: {parameters.adapter}</div> : null}
            {!loading && adapters.length === 0 ? <div className="rounded-md border border-dashed p-3 text-xs text-muted-foreground">NO ETHERCAT-CAPABLE ADAPTERS WERE FOUND. ON WINDOWS, INSTALL NPCAP, RESTART DARTWIC, THEN REFRESH.</div> : null}
        </div>
        <div className="space-y-2"><Label>FRAME RECEIVE TIMEOUT (µs)</Label><Input type="number" min="50" step="50" value={parameters.receive_timeout_us ?? 500} onChange={(event) => update("receive_timeout_us", Number(event.target.value))}/>
            <div className="text-xs text-muted-foreground">500 µs is a practical starting point for a 1 ms cycle. Increase it only when the adapter or simulated network needs more response time.</div>
        </div>
        <div className="rounded-md border bg-muted/30 p-3 text-xs text-muted-foreground">
            Connect this adapter to the TwinCAT EtherCAT Simulation adapter. One running cyclic task owns the master exclusively; stop that task before changing adapters.
        </div>
    </div>;
}
