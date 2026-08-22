import React from "../react.ts";
import { getHostApi } from "./host.ts";

type Resolver = (hostApi: ReturnType<typeof getHostApi>) => any;

export type HostComponent<Props extends Record<string, unknown>> = ((props: Props) => any) & {displayName: string};

export function createHostComponent<Props extends Record<string, unknown>>(
    displayName: string,
    resolver: Resolver,
): HostComponent<Props> {
    function HostComponent(props: Props) {
        const Component = resolver(getHostApi());
        return (React as any).createElement(Component, props);
    }

    HostComponent.displayName = displayName;
    return HostComponent as HostComponent<Props>;
}
