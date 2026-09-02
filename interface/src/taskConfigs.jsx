import React from "../sdk/react.ts";
import {Button, Label, ScrollArea, ScrollBar, Select, SelectContent, SelectItem, SelectTrigger, SelectValue} from "../sdk/ui/general.ts";
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
function MappingRow({mapping, entries, onChange, onRemove}) {
    const selected = entries.find((entry) => entryKey(entry) === mapping.entry_key);
    return <div className="space-y-3 rounded-md border p-3">
        <div className="flex items-center gap-2"><div className="min-w-0 flex-1">
            <Select value={mapping.entry_key || ""} onValueChange={(key) => {
                const entry = entries.find((candidate) => entryKey(candidate) === key);
                if (entry) onChange({...entry, entry_key: key, channel: mapping.channel || "", scale: 1, offset: 0});
            }}><SelectTrigger><SelectValue placeholder="SELECT A DISCOVERED PDO ENTRY"/></SelectTrigger><SelectContent>
                {entries.map((entry) => <SelectItem key={entryKey(entry)} value={entryKey(entry)}>{labelFor(entry)}</SelectItem>)}
            </SelectContent></Select>
        </div><Button variant="ghost" onClick={onRemove}>REMOVE</Button></div>
        <ChannelComboBox mode={selected?.direction === "channel_to_device" ? "read" : "write"} showFieldSelector={false}
            initialValue={mapping.channel || ""} placeholder="SELECT FIXED RAPID CHANNEL"
            onSelect={(value) => onChange({...mapping, channel: convertChannelReferenceToChannelName(value)})} className="w-full"/>
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
    const payload = React.useMemo(() => ({module_instance_name: instance,
        mappings: mappings.filter((mapping) => mapping.entry_key && mapping.channel).map(({row_key, ...mapping}) => mapping)}), [instance, mappings]);
    const initial = React.useMemo(() => ({module_instance_name: task.arguments?.module_instance_name || "", mappings: task.arguments?.mappings || []}), [task]);
    const dirty = JSON.stringify(payload) !== JSON.stringify(initial);
    async function scan() {
        if (!instance) return setError("SELECT AN ETHERCAT MASTER FIRST.");
        setScanning(true); setError("");
        try { setTopology(unwrap(await operation("ethercat.scan", {module_instance_name: instance}, 30000))); }
        catch (reason) { setError(reason instanceof Error ? reason.message : String(reason)); }
        finally { setScanning(false); }
    }
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
        taskEditor?.register?.({isDirty: dirty, isSaving: saving, canSave: true, errorMessage: error,
            saveLabel: "SAVE", cancelLabel: "CANCEL", onSave: saveTask, onCancel: onClose});
    }, [taskEditor, dirty, saving, error, payload]);
    return <div className="flex h-full min-h-0 flex-col gap-4">
        <div className="space-y-2"><Label>ETHERCAT MASTER</Label><div className="flex gap-2">
            <div className="min-w-0 flex-1"><ModuleInstanceSelect pluginId="ethercat" moduleTypeIds={["master"]} value={instance}
                onValueChange={(value) => { setInstance(value); setTopology(null); }} placeholder="SELECT ONE MASTER"/></div>
            <Button variant="outline" onClick={scan}>{scanning ? "SCANNING…" : "SCAN BUS"}</Button>
        </div><div className="text-xs text-muted-foreground">Scan after configuring the master. Schedule this periodic task at 1 ms for a 1 kHz exchange.</div></div>
        <div className="flex items-center justify-between"><Label>CYCLIC PDO MAPPINGS</Label><Button variant="outline" disabled={!topology}
            onClick={() => setMappings((current) => current.concat([{row_key: `new-${nextRow.current++}`, entry_key: "", channel: ""}]))}>ADD MAPPING</Button></div>
        <ScrollArea className="min-h-0 flex-1" type="always"><div className="space-y-3 pr-4">
            {!topology ? <div className="rounded-md border border-dashed p-4 text-sm text-muted-foreground">SELECT A MASTER AND SCAN THE BUS TO LOAD SLAVE AND PDO DROPDOWNS.</div> : null}
            {topology && entries.length === 0 ? <div className="rounded-md border border-dashed p-4 text-sm text-muted-foreground">NO MAPPED PDO ENTRIES WERE DISCOVERED.</div> : null}
            {mappings.map((mapping, index) => <MappingRow key={mapping.row_key || index} mapping={mapping} entries={entries}
                onChange={(next) => setMappings((current) => current.map((value, item) => item === index ? {...next, row_key: mapping.row_key} : value))}
                onRemove={() => setMappings((current) => current.filter((_, item) => item !== index))}/>) }
        </div><ScrollBar orientation="vertical"/></ScrollArea>
    </div>;
}
