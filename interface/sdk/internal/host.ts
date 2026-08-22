import type { InterfacePluginHostApi } from "../types/index.ts";

let currentHostApi: InterfacePluginHostApi | null = null;

export function setHostApi(hostApi: InterfacePluginHostApi) {
    currentHostApi = hostApi;
}

export function getHostApi(): InterfacePluginHostApi {
    if (!currentHostApi) {
        throw new Error("The DARTWIC interface plugin host API is not available yet.");
    }

    return currentHostApi;
}
