import { createHostComponent } from "../internal/createHostComponent.ts";
import { getHostApi } from "../internal/host.ts";
import type {ChannelComboBoxProps, ConfigurableInputProps, ManualRefreshButtonProps} from "./types.ts";

/** Selects a DARTWIC channel and, optionally, one of its fields. @dartwic-reference @category DARTWIC UI Components */
export const ChannelComboBox = createHostComponent<ChannelComboBoxProps>("ChannelComboBox", (hostApi) => hostApi.helpers.ChannelComboBox);
/** Edits a configurable literal, channel reference, or expression value. @dartwic-reference @category DARTWIC UI Components */
export const ConfigurableInput = createHostComponent<ConfigurableInputProps>("ConfigurableInput", (hostApi) => hostApi.helpers.ConfigurableInput);
/** Renders the standard refresh icon button with an in-progress state. @dartwic-reference @category DARTWIC UI Components */
export const ManualRefreshButton = createHostComponent<ManualRefreshButtonProps>("ManualRefreshButton", (hostApi) => hostApi.helpers.ManualRefreshButton);

/**
 * Converts a channel value path such as `portal/channel.value` into its channel name.
 *
 * @dartwic-reference
 * @category DARTWIC UI Components
 * @param value Channel value path to normalize.
 * @returns The channel portion of the path.
 */
export function convertChannelValuePathToChannelName(value: string) {
    const convert = getHostApi().helpers.convertChannelValuePathToChannelName;
    if (typeof convert !== "function") {
        throw new Error("The interface host does not provide convertChannelValuePathToChannelName.");
    }
    return String(convert(value));
}

export type {ChannelComboBoxProps, ConfigurableInputProps, ManualRefreshButtonProps} from "./types.ts";
