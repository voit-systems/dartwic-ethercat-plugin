import React from "../sdk/react.ts";
import {Button, Input, Label, ScrollArea, ScrollBar, Select, SelectContent, SelectItem, SelectTrigger, SelectValue} from "../sdk/ui/general.ts";
import {ChannelComboBox, convertChannelReferenceToChannelName, ModuleInstanceSelect} from "../sdk/ui/dartwic.ts";

function unwrap(result) {
    if (result?.error) throw new Error(result?.payload?.error || "EtherCAT operation failed.");
    return result?.payload ?? result;
}
function flattenEntries(topology) {
    const entries = [];
    for (const slave of topology?.slaves || []) {
        for (const entry of slave.outputs || []) entries.push({...entry, slave_position: slave.position, slave_name: slave.name, direction: "channel_to_device"});
        for (const entry of slave.inputs || []) entries.push({...entry, slave_position: slave.position, slave_name: slave.name, direction: "device_to_channel"});
    }
    return entries;
}
function entryKey(entry) { return `${entry.direction}:${entry.slave_position}:${entry.pdo_index}:${entry.index}:${entry.subindex}:${entry.bit_offset}`; }
function labelFor(entry) {
    const arrow = entry.direction === "channel_to_device" ? "RAPID → DEVICE" : "DEVICE → RAPID";
    const object = `0x${Number(entry.index).toString(16).padStart(4, "0")}:${entry.subindex}`;
    return `${arrow} · S${entry.slave_position} ${entry.slave_name} · ${object} ${entry.name} (${entry.data_type})`;
}
function MappingRow({mapping, entries, index, onChange, onRemove}) {
    const selected = entries.find((entry) => entryKey(entry) === mapping.entry_key);
    const direction = selected?.direction || mapping.direction;
    const availableEntries = direction ? entries.filter((entry) => entry.direction === direction) : entries;
    const directionLabel = direction === "channel_to_device" ? "RAPID → DEVICE" : direction === "device_to_channel" ? "DEVICE → RAPID" : "SELECT DIRECTION";
    return <div className="space-y-3 rounded-md border bg-muted/10 p-3">
        <div className="flex items-center justify-between gap-3">
            <div className="flex items-center gap-2"><span className="text-xs font-semibold">MAPPING {index + 1}</span><span className="rounded border px-2 py-0.5 text-[10px] text-muted-foreground">{directionLabel}</span></div>
            <Button variant="ghost" onClick={onRemove}>REMOVE</Button>
        </div>
        <div className="space-y-2"><Label>PDO ENTRY</Label>
            <Select value={mapping.entry_key || ""} onValueChange={(key) => {
                const entry = entries.find((candidate) => entryKey(candidate) === key);
                if (entry) onChange({...entry, entry_key: key, channel: mapping.channel || "", scale: mapping.scale ?? 1, offset: mapping.offset ?? 0});
            }}><SelectTrigger><SelectValue placeholder="SELECT A DISCOVERED PDO ENTRY"/></SelectTrigger><SelectContent>
                {availableEntries.map((entry) => <SelectItem key={entryKey(entry)} value={entryKey(entry)}>{labelFor(entry)}</SelectItem>)}
            </SelectContent></Select>
        </div>
        <div className="space-y-2"><Label>{direction === "channel_to_device" ? "COMMAND SOURCE CHANNEL" : "TELEMETRY DESTINATION CHANNEL"}</Label>
            <ChannelComboBox mode={direction === "channel_to_device" ? "read" : "write"} showFieldSelector={false}
                initialValue={mapping.channel || ""} placeholder="SELECT FIXED RAPID CHANNEL"
                onSelect={(value) => onChange({...mapping, channel: convertChannelReferenceToChannelName(value)})} className="w-full"/>
        </div>
        <div className="grid grid-cols-2 gap-3">
            <div className="space-y-2"><Label>SCALE</Label><Input type="number" step="any" value={mapping.scale ?? 1} onChange={(event) => onChange({...mapping, scale: Number(event.target.value)})}/></div>
            <div className="space-y-2"><Label>OFFSET</Label><Input type="number" step="any" value={mapping.offset ?? 0} onChange={(event) => onChange({...mapping, offset: Number(event.target.value)})}/></div>
        </div>
        {!mapping.entry_key || !mapping.channel ? <div className="text-xs text-yellow">SELECT BOTH A PDO ENTRY AND RAPID CHANNEL TO COMPLETE THIS MAPPING.</div> : null}
    </div>;
}

export function EthercatTaskConfig({task, operation, onSaved, onClose, taskEditor}) {
    const [instance, setInstance] = React.useState(task.arguments?.module_instance_name || "");
    const [mappings, setMappings] = React.useState(() => (task.arguments?.mappings || []).map((mapping, index) => ({...mapping, entry_key: mapping.entry_key || entryKey(mapping), row_key: `saved-${index}`})));
    const [topology, setTopology] = React.useState(null);
    const [scanning, setScanning] = React.useState(false);
    const [saving, setSaving] = React.useState(false);
    const [error, setError] = React.useState("");
    const nextRow = React.useRef(0);
    const initialScanStarted = React.useRef(false);
    const entries = React.useMemo(() => {
        const merged = flattenEntries(topology);
        const knownKeys = new Set(merged.map(entryKey));
        for (const mapping of mappings) {
            if (!mapping.entry_key || knownKeys.has(mapping.entry_key)) continue;
            merged.push(mapping);
            knownKeys.add(mapping.entry_key);
        }
        return merged;
    }, [topology, mappings]);
    const payload = React.useMemo(() => ({...(task.arguments || {}), module_instance_name: instance,
        mappings: mappings.filter((mapping) => mapping.entry_key && mapping.channel).map(({row_key, ...mapping}) => mapping)}), [task.arguments, instance, mappings]);
    const initial = React.useMemo(() => ({...(task.arguments || {}), module_instance_name: task.arguments?.module_instance_name || "", mappings: task.arguments?.mappings || []}), [task]);
    const dirty = JSON.stringify(payload) !== JSON.stringify(initial);
    const complete = mappings.length > 0 && payload.mappings.length === mappings.length;
    const scan = React.useCallback(async () => {
        if (!instance) return setError("SELECT AN ETHERCAT MASTER FIRST.");
        setScanning(true); setError("");
        try { setTopology(unwrap(await operation("ethercat.scan", {module_instance_name: instance}, 30000))); }
        catch (reason) { setError(reason instanceof Error ? reason.message : String(reason)); }
        finally { setScanning(false); }
    }, [instance, operation]);
    React.useEffect(() => {
        if (initialScanStarted.current || !instance || mappings.length === 0) return;
        initialScanStarted.current = true;
        void scan();
    }, [instance, mappings.length, scan]);
    async function saveTask() {
        if (!instance) return setError("SELECT AN ETHERCAT MASTER.");
        if (payload.mappings.length === 0) return setError("ADD AT LEAST ONE COMPLETE PDO MAPPING.");
        setSaving(true); setError("");
        try {
            const result = await operation("dartwic/create-task", {portal_name: task.portal, task_name: task.name, task_type: task.task_type, arguments: payload}, 30000);
            if (result?.error) return setError(result?.payload?.error || "FAILED TO SAVE TASK.");
            await onSaved?.(); await onClose?.();
        } finally { setSaving(false); }
    }
    React.useEffect(() => {
        taskEditor?.register?.({isDirty: dirty, isSaving: saving, canSave: Boolean(instance) && complete, errorMessage: error,
            saveLabel: "SAVE", cancelLabel: "CANCEL", onSave: saveTask, onCancel: onClose});
    }, [taskEditor, dirty, saving, error, payload, instance, complete]);
    const slaveCount = topology?.slaves?.length || 0;
    const processImage = topology?.process_image || {};
    return <div className="flex h-full min-h-0 flex-col gap-4">
        <div className="rounded-md border bg-muted/20 p-3"><div className="space-y-2"><Label>ETHERCAT MASTER</Label><div className="flex gap-2">
            <div className="min-w-0 flex-1"><ModuleInstanceSelect pluginId="ethercat" moduleTypeIds={["master"]} value={instance}
                onValueChange={(value) => { if (value !== instance) setMappings([]); setInstance(value); setTopology(null); }} placeholder="SELECT ONE MASTER"/></div>
            <Button variant="outline" disabled={!instance || scanning} onClick={scan}>{scanning ? "SCANNING…" : topology ? "REFRESH TOPOLOGY" : "SCAN BUS"}</Button>
        </div>
        {topology ? <div className="flex flex-wrap gap-x-4 gap-y-1 text-xs text-muted-foreground"><span>{slaveCount} SLAVE{slaveCount === 1 ? "" : "S"}</span><span>{processImage.outputs_bytes || 0} OUTPUT BYTES</span><span>{processImage.inputs_bytes || 0} INPUT BYTES</span></div>
            : <div className="text-xs text-muted-foreground">Connect the configured adapter to the TwinCAT simulation network, then scan to discover PDOs.</div>}
        </div></div>
        <div className="flex flex-wrap items-center justify-between gap-2"><div><Label>CYCLIC PDO MAPPINGS</Label><div className="mt-1 text-xs text-muted-foreground">{mappings.length} CONFIGURED</div></div><div className="flex gap-2">
            <Button variant="outline" disabled={!topology} onClick={() => setMappings((current) => current.concat([{row_key: `new-${nextRow.current++}`, direction: "channel_to_device", entry_key: "", channel: "", scale: 1, offset: 0}]))}>ADD COMMAND</Button>
            <Button variant="outline" disabled={!topology} onClick={() => setMappings((current) => current.concat([{row_key: `new-${nextRow.current++}`, direction: "device_to_channel", entry_key: "", channel: "", scale: 1, offset: 0}]))}>ADD TELEMETRY</Button>
        </div></div>
        <ScrollArea className="min-h-0 flex-1" type="always"><div className="space-y-3 pr-4">
            {!topology && mappings.length === 0 ? <div className="rounded-md border border-dashed p-4 text-sm text-muted-foreground">SELECT A MASTER AND SCAN THE BUS TO LOAD SLAVE AND PDO DROPDOWNS.</div> : null}
            {!topology && mappings.length > 0 ? <div className="rounded-md border border-dashed p-3 text-xs text-muted-foreground">SHOWING SAVED PDO MAPPINGS. SCAN THE BUS TO REFRESH THE AVAILABLE ENTRIES.</div> : null}
            {topology && entries.length === 0 ? <div className="rounded-md border border-dashed p-4 text-sm text-muted-foreground">NO MAPPED PDO ENTRIES WERE DISCOVERED.</div> : null}
            {topology && mappings.length === 0 ? <div className="rounded-md border border-dashed p-4 text-sm text-muted-foreground">THE BUS IS READY. ADD A COMMAND OR TELEMETRY MAPPING.</div> : null}
            {mappings.map((mapping, index) => <MappingRow key={mapping.row_key || index} mapping={mapping} entries={entries} index={index}
                onChange={(next) => setMappings((current) => current.map((value, item) => item === index ? {...next, row_key: mapping.row_key} : value))}
                onRemove={() => setMappings((current) => current.filter((_, item) => item !== index))}/>) }
        </div><ScrollBar orientation="vertical"/></ScrollArea>
    </div>;
}
