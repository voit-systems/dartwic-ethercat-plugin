import React from "../sdk/react.ts";
import {Button, Input, Label, ScrollArea, ScrollBar} from "../sdk/ui/general.ts";
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
function PdoEntrySearch({mapping, entries, onChange}) {
    const selected = entries.find((entry) => entryKey(entry) === mapping.entry_key);
    const availableEntries = entries.filter((entry) => entry.direction === mapping.direction);
    const selectedLabel = selected ? labelFor(selected) : "";
    const [query, setQuery] = React.useState(selectedLabel);
    const listId = React.useId();
    React.useEffect(() => { setQuery(selectedLabel); }, [selectedLabel]);
    return <>
        <Input list={listId} value={query} placeholder="SEARCH PDO ENTRY" onChange={(event) => {
            const value = event.target.value;
            setQuery(value);
            const entry = availableEntries.find((candidate) => labelFor(candidate) === value);
            if (entry) onChange({...entry, entry_key: entryKey(entry), channel: mapping.channel || "", scale: mapping.scale ?? 1, offset: mapping.offset ?? 0});
        }} onBlur={() => {
            if (!availableEntries.some((entry) => labelFor(entry) === query)) setQuery(selectedLabel);
        }}/>
        <datalist id={listId}>{availableEntries.map((entry) => <option key={entryKey(entry)} value={labelFor(entry)}/>)}</datalist>
    </>;
}
function MappingRow({mapping, entries, onChange, onRemove}) {
    const direction = mapping.direction;
    return <div className="grid grid-cols-[minmax(0,1.5fr)_minmax(0,1fr)_auto] items-center gap-2 border-b py-2 last:border-b-0">
        <PdoEntrySearch mapping={mapping} entries={entries} onChange={onChange}/>
        <ChannelComboBox key={mapping.channel || mapping.row_key} mode={direction === "channel_to_device" ? "read" : "write"} showFieldSelector={false}
            initialValue={mapping.channel || ""} placeholder={mapping.channel || "SEARCH CHANNEL"} editableTrigger={true}
            onSelect={(value) => onChange({...mapping, channel: convertChannelReferenceToChannelName(value)})} className="w-full"/>
        <Button className="h-10 shrink-0 px-3" variant="ghost" onClick={onRemove}>DELETE</Button>
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
    return <div className="flex h-full min-h-0 flex-col gap-4">
        <div className="space-y-2"><Label>ETHERCAT MASTER</Label><div className="flex items-center gap-2">
            <div className="min-w-0 flex-1"><ModuleInstanceSelect pluginId="ethercat" moduleTypeIds={["master"]} value={instance}
                onValueChange={(value) => { if (value !== instance) setMappings([]); setInstance(value); setTopology(null); }} placeholder="SELECT ONE MASTER"/></div>
            <Button className="h-10 shrink-0 px-4" variant="outline" disabled={!instance || scanning} onClick={scan}>{scanning ? "SCANNING…" : "SCAN BUS"}</Button>
        </div></div>
        <div className="flex items-center justify-between gap-2"><Label>PDO MAPPINGS</Label><div className="flex items-center gap-2">
            <Button className="h-9 px-3" variant="outline" disabled={!topology} onClick={() => setMappings((current) => current.concat([{row_key: `new-${nextRow.current++}`, direction: "channel_to_device", entry_key: "", channel: "", scale: 1, offset: 0}]))}>ADD COMMAND</Button>
            <Button className="h-9 px-3" variant="outline" disabled={!topology} onClick={() => setMappings((current) => current.concat([{row_key: `new-${nextRow.current++}`, direction: "device_to_channel", entry_key: "", channel: "", scale: 1, offset: 0}]))}>ADD TELEMETRY</Button>
        </div></div>
        <ScrollArea className="min-h-0 flex-1" type="always"><div className="pr-3">
            {mappings.length === 0 ? <div className="py-3 text-sm text-muted-foreground">NO MAPPINGS</div> : null}
            {mappings.map((mapping, index) => <MappingRow key={mapping.row_key || index} mapping={mapping} entries={entries}
                onChange={(next) => setMappings((current) => current.map((value, item) => item === index ? {...next, row_key: mapping.row_key} : value))}
                onRemove={() => setMappings((current) => current.filter((_, item) => item !== index))}/>) }
        </div><ScrollBar orientation="vertical"/></ScrollArea>
    </div>;
}
