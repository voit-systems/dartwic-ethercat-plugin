import { getHostApi } from "../internal/host.ts";

/** Combines conditional class values with the host's Tailwind conflict resolution. @dartwic-reference @category Hooks and Utilities */
export function cn(...inputs: unknown[]) {
    return getHostApi().sdk.styling.cn(...inputs);
}
