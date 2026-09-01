import {definePlugin} from "../sdk/index.ts";
import React from "../sdk/react.ts";
import {EthercatModuleConfig} from "./moduleConfigs.jsx";
import {EthercatTaskCard} from "./taskCards.jsx";
import {EthercatTaskConfig} from "./taskConfigs.jsx";

function EthercatIcon(props) {
    return <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" {...props}>
        <path d="M3 7h5v10H3zM16 7h5v10h-5z"/><path d="M8 10h8M8 14h8M5 4v3M19 4v3M5 17v3M19 17v3"/>
    </svg>;
}

export default definePlugin({
    id: "ethercat",
    name: "EtherCAT",
    register(registry) {
        registry.addTaskUi({id: "cycle", name: "EtherCAT Cyclic I/O", card: EthercatTaskCard, editor: EthercatTaskConfig});
        registry.addModuleUi({id: "master", name: "EtherCAT Master", icon: EthercatIcon, panel: EthercatModuleConfig});
    },
});
