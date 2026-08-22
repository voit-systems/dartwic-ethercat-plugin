/** Supported resource presentation modes. @dartwic-reference @category Resources */
export type ResourceType = "component" | "directory";

/** Complete configuration for an interface resource. @dartwic-reference @category Resources */
export interface ResourceDefinition {
    name: string;
    label?: string;
    icon?: any;
    type: ResourceType;
    component?: any;
    show_in_resource_tabs?: boolean;
    file_extension?: string;
    file_extensions?: string[];
    excluded_file_names?: string[];
    excluded_paths?: string[];
    show_file_extensions?: boolean;
    enable_directory_resource_groups?: boolean;
    resource_on_create_file_data?: string;
    resource_on_create_file_dialog?: any;
    resource_on_delete_function?: any;
    get_tree_file_icon_src?: any;
    tree_file_icon?: any;
    tree_directory_icon?: any;
    file_icons?: Record<string, any>;
    context_label?: string;
    context_config?: any;
}
