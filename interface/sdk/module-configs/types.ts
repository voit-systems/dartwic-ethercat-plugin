/** Props passed to a module configuration component. @dartwic-reference @category Module Configuration */
export interface ModuleConfigProps {
    instanceConfig: any;
    setInstanceConfig: (nextValue: any) => void;
    save: () => Promise<void> | void;
    moduleConfig?: any;
}

/** Component registered as a module configuration editor. @dartwic-reference @category Module Configuration */
export interface ModuleConfigDefinition {
    component: (props: ModuleConfigProps) => any;
}
