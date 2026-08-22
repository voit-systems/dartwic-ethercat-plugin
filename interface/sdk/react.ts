import { getHostApi } from "./internal/host.ts";

/** Shared React runtime proxy used to avoid loading a second React instance. @dartwic-reference @category Runtime */
const ReactProxy: any = new Proxy({}, {
    get(_target, property) {
        return (getHostApi().React as any)?.[property];
    }
});

export default ReactProxy;
