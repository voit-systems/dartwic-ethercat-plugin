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
            canSave: parameters.mode !== "hardware" || Boolean(parameters.adapter), errorMessage: error,
            saveLabel: "SAVE CONFIG", onSave: async () => { await save(); setSaved(parameters); }});
    }, [moduleEditor, isDirty, parameters, error, save]);
    const mode = parameters.mode || "simulator";
    const hardwareAdapters = adapters.filter((adapter) => adapter.kind === "hardware");
    return <div className="space-y-5">
        <div className="grid grid-cols-1 gap-4 md:grid-cols-2">
            <div className="space-y-2"><Label>BACKEND</Label>
                <Select value={mode} onValueChange={(value) => update("mode", value)}><SelectTrigger><SelectValue/></SelectTrigger><SelectContent>
                    <SelectItem value="simulator">KICKCAT SIMULATOR</SelectItem><SelectItem value="hardware">PHYSICAL ETHERCAT ADAPTER</SelectItem>
                </SelectContent></Select>
            </div>
            {mode === "hardware" ? <div className="space-y-2">
                <div className="flex items-center justify-between"><Label>NETWORK ADAPTER</Label><Button variant="outline" onClick={loadAdapters}>{loading ? "LOADING…" : "REFRESH"}</Button></div>
                <Select value={parameters.adapter || ""} onValueChange={(value) => update("adapter", value)}><SelectTrigger><SelectValue placeholder="SELECT AN ADAPTER"/></SelectTrigger><SelectContent>
                    {hardwareAdapters.map((adapter) => <SelectItem key={adapter.id} value={adapter.id}>{adapter.name || adapter.id}</SelectItem>)}
                </SelectContent></Select>
                {hardwareAdapters.length === 0 ? <div className="text-xs text-muted-foreground">NO COMPATIBLE ADAPTERS FOUND. WINDOWS REQUIRES NPCAP.</div> : null}
            </div> : <div className="space-y-2"><Label>SIMULATED DEVICE PROFILE</Label>
                <Select value={parameters.simulator_profile || "standard_io"} onValueChange={(value) => update("simulator_profile", value)}><SelectTrigger><SelectValue/></SelectTrigger><SelectContent>
                    <SelectItem value="standard_io">STANDARD I/O + ECHO</SelectItem>
                </SelectContent></Select>
            </div>}
            <div className="space-y-2"><Label>RECEIVE TIMEOUT (µs)</Label><Input type="number" min="50" value={parameters.receive_timeout_us ?? 500} onChange={(event) => update("receive_timeout_us", Number(event.target.value))}/></div>
        </div>
        <div className="rounded-md border bg-muted/30 p-3 text-sm text-muted-foreground">Each master is exclusively owned by one cyclic task while running. Stopping the task stops exchanges without forcing output values.</div>
    </div>;
}
