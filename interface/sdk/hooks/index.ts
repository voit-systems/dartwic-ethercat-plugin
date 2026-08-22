import { getHostApi } from "../internal/host.ts";

/** Returns the host's shared DARTWIC React context hook result. @dartwic-reference @category Hooks and Utilities */
export function useDartwic() {
    return getHostApi().useDartwic();
}
