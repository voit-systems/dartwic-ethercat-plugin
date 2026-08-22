/** Props passed to a plugin settings component. @dartwic-reference @category Plugin Settings */
export interface PluginSettingsProps {
    operation?: (...args: any[]) => Promise<any>;
    plugin?: any;
    pluginGroup?: any;
}

/** Component registered as a plugin settings panel. @dartwic-reference @category Plugin Settings */
export interface PluginSettingsDefinition {
    component: (props: PluginSettingsProps) => any;
}
