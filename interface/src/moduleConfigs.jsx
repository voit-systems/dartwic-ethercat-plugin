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
    return <div className="space-y-4">
        <div className="space-y-2">
            <Label>ETHERCAT NETWORK ADAPTER</Label>
            <div className="flex items-center gap-2">
                <div className="min-w-0 flex-1"><Select value={parameters.adapter || ""} onValueChange={(value) => update("adapter", value)}><SelectTrigger><SelectValue placeholder="SELECT ADAPTER"/></SelectTrigger><SelectContent>
                    {adapterOptions.map((adapter) => <SelectItem key={adapter.id} value={adapter.id}>{adapter.name || adapter.id}</SelectItem>)}
                </SelectContent></Select></div>
                <Button className="h-10 shrink-0 px-4" variant="outline" disabled={loading} onClick={loadAdapters}>{loading ? "REFRESHING…" : "REFRESH"}</Button>
            </div>
            {!loading && adapters.length === 0 ? <div className="text-xs text-muted-foreground">NO ADAPTERS FOUND. WINDOWS REQUIRES NPCAP.</div> : null}
        </div>
        <div className="space-y-2"><Label>FRAME RECEIVE TIMEOUT (µs)</Label><Input type="number" min="50" step="50" value={parameters.receive_timeout_us ?? 500} onChange={(event) => update("receive_timeout_us", Number(event.target.value))}/>
        </div>
    </div>;
}
