#ifndef SDK_API_H
#define SDK_API_H

#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace DARTWIC::Modules {
    class BaseModule;
}

namespace DARTWIC::API {
    /**
     * Engine-side link used by a Share type.
     *
     * A Share type only moves complete JSON frames. The engine owns RAPID and
     * ARGUS synchronization, routing, diagnostics, and protocol semantics.
     */
    class ShareTransport {
    public:
        using ReceiveHandler = std::function<void(nlohmann::json frame)>;

        virtual ~ShareTransport() = default;
        virtual void start(ReceiveHandler receive) = 0;
        virtual bool send(const nlohmann::json& frame) = 0;
        virtual bool sendBatch(const std::vector<nlohmann::json>& frames) {
            for (const auto& frame : frames) {
                if (!send(frame)) return false;
            }
            return true;
        }
        virtual void stop() = 0;
    };

    using ShareTransportPtr = std::shared_ptr<ShareTransport>;

    /**
     * Execution shape used by a registered task type.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     */
    enum class TaskStructure {
        Unknown,
        Periodic,
        StateMachine,
        Sequence,
        Worker
    };

    /**
     * Addressable fields stored for each RAPID channel.
     *
     * @dartwic-reference
     * @category Channels
     */
    enum class ChannelField {
        VALUE,
        COMMANDED_BY,
        TIMESTAMP,
        UNITS,
        STALE_TIMEOUT,
        RECORD_MODE,
        DATA_FRAME,
        CONTROL_POLICY,
        CONTROL_OWNER,
        ACTIVE_CONTROLLER,
        LINKED_CALCULATION_SCRIPTS,
        VALUE_OPTIONS
    };

    /**
     * Controls when channel values are recorded to persistence.
     *
     * @dartwic-reference
     * @category Channels
     */
    enum class RecordMode {
        OnValueChange,
        Never,
        EveryValue
    };

    /**
     * Authority policy applied to commandable channels.
     *
     * @dartwic-reference
     * @category Channel Authority
     */
    enum class ControlPolicy {
        Free,
        Automatic,
        ManualOverride,
        ObserveOnly
    };

    struct ChannelValueOption {
        double value = 0.0;
        std::string label;
    };

    struct ChannelCalculationLink {
        std::string script;
        std::string relationship;
        uint32_t line_number = 0;
    };

    /**
     * Value types accepted by channel read, write, and authority APIs.
     *
     * @dartwic-reference
     * @category Channels
     */
    using ChannelValue = std::variant<double, int, std::string, bool, RecordMode, ControlPolicy,
        std::vector<ChannelValueOption>, std::vector<ChannelCalculationLink>>;

    /**
     * Handler for a plugin-defined TEMPEST extension operation.
     *
     * Plugin-defined operations are not part of the official DARTWIC operation catalog.
     *
     * @dartwic-reference
     * @category Operations
     */
    using OperationHandler = std::function<nlohmann::json(const nlohmann::json& payload)>;

    /**
     * Native callback exposed to DCode through the plugin SDK.
     *
     * @dartwic-reference
     * @category DCode
     */
    using DCodeFunctionHandler = std::function<nlohmann::json(const nlohmann::json& payload)>;

    /**
     * Describes one input or output in a plugin-provided DCode function.
     *
     * @dartwic-reference
     * @category DCode
     */
    struct DCodeFunctionArgument {
        std::string name;
        std::string type;
        std::string doc;
        bool required = false;
    };

    /**
     * Driver-host periodic task ABI.
     *
     * @dartwic-reference-exclude driver-descoped
     */
    struct DriverPeriodicTaskRegistration {
        const char* task_type = nullptr;
        const char* task_name = nullptr;
        void* context = nullptr;
        void (*on_start)(void* context, double elapsed_seconds) = nullptr;
        void (*on_task)(void* context, double elapsed_seconds) = nullptr;
        void (*on_end)(void* context, double elapsed_seconds) = nullptr;
    };

    /**
     * Driver-host state-machine task ABI.
     *
     * @dartwic-reference-exclude driver-descoped
     */
    struct DriverStateMachineTaskRegistration {
        const char* task_type = nullptr;
        const char* task_name = nullptr;
        const char* states_json = nullptr;
        void* context = nullptr;
        void (*on_start)(void* context, double elapsed_seconds) = nullptr;
        void (*on_task)(void* context, double elapsed_seconds) = nullptr;
        void (*on_end)(void* context, double elapsed_seconds) = nullptr;
    };

    /**
     * Driver runtime host ABI. This ABI intentionally has no legacy grouping or path fields.
     *
     * @dartwic-reference-exclude driver-descoped
     */
    struct DriverPluginHostApi {
        void* host_context = nullptr;
        double (*query_channel_value)(void* host_context, const char* channel_name, double default_value) = nullptr;
        void (*upsert_channel_value)(void* host_context, const char* channel_name, double value) = nullptr;
        bool (*register_periodic_task)(void* host_context, const DriverPeriodicTaskRegistration* registration) = nullptr;
        bool (*register_state_machine_task)(void* host_context, const DriverStateMachineTaskRegistration* registration) = nullptr;
        bool (*register_dcode_function)(void* host_context, const char* function_name, const char* doc, const char* input_arguments_json, const char* output_arguments_json, void* function_context, const char* (*callback)(void* function_context, const char* payload_json)) = nullptr;
        const char* (*call_dcode_function)(void* host_context, const char* function_name, const char* payload_json) = nullptr;
        void (*free_json_string)(void* host_context, const char* value) = nullptr;
        void (*log_message)(void* host_context, const char* message) = nullptr;
    };

    struct TaskTypeDefinition;

    /**
     * Identity, structure, presentation, and defaults for a plugin task type.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     */
    struct TaskTypeMetadata {
        std::string task_type;
        TaskStructure structure = TaskStructure::Unknown;
        std::string icon_url;
        std::string exposed_from;
        std::string expected_plugin_id;
        nlohmann::json default_arguments = nlohmann::json::object();
    };

    /**
     * Live task context passed to plugin task lifecycle callbacks.
     *
     * Runtime context values persist for the lifetime of one task runtime and can be
     * used to share plugin-owned state between start, task, end, and cleanup callbacks.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     */
    class TaskRuntime {
    public:
        virtual ~TaskRuntime() = default;

        virtual const std::string& getTaskName() const = 0;
        virtual const std::string& getTaskType() const = 0;
        virtual const nlohmann::json& getMetadata() const = 0;
        virtual const nlohmann::json& getArguments() const = 0;
        virtual double getElapsedSeconds() const = 0;
        virtual bool isStopRequested() const = 0;

        virtual void setRuntimeContext(const std::string& key, std::shared_ptr<void> value) = 0;
        virtual std::shared_ptr<void> getRuntimeContext(const std::string& key) const = 0;
        virtual void removeRuntimeContext(const std::string& key) = 0;
        virtual void clearRuntimeContext() = 0;

        template <typename T>
        void setTypedRuntimeContext(const std::string& key, const std::shared_ptr<T>& value) {
            setRuntimeContext(key, std::static_pointer_cast<void>(value));
        }

        template <typename T>
        std::shared_ptr<T> getTypedRuntimeContext(const std::string& key) const {
            return std::static_pointer_cast<T>(getRuntimeContext(key));
        }
    };

    /**
     * Callback used for task start and end lifecycle phases.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     */
    using TaskLifecycleFunction = std::function<void(const TaskTypeDefinition&, TaskRuntime&)>;
    /**
     * Callback used for repeated task execution with elapsed seconds.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     */
    using TaskLoopFunction = std::function<void(const TaskTypeDefinition&, TaskRuntime&, double)>;
    /**
     * Callback used to release runtime state after task execution ends.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     */
    using TaskCleanupFunction = std::function<void(TaskRuntime&)>;

    /**
     * Complete registration definition for a plugin task type.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     */
    struct TaskTypeDefinition {
        TaskTypeMetadata metadata;
        TaskLifecycleFunction on_start;
        TaskLoopFunction on_task;
        TaskLifecycleFunction on_end;
        TaskCleanupFunction cleanup;
    };

    /**
     * Lightweight identity returned when enumerating live module instances.
     *
     * @dartwic-reference
     * @category Modules
     */
    struct ModuleInstanceSummary {
        std::string name;
        std::string plugin_id;
        std::string module_type_id;
        std::string resource_path;
    };

    /**
     * Declares a module type that a plugin can instantiate.
     *
     * @dartwic-reference
     * @category Modules
     */
    struct ModuleTypeDefinition {
        std::string id;
        std::string name;
        std::string config_path = "module_config.json";
        std::string default_parameters_path = "default_parameters.json";
    };

    /**
     * Declares a plugin-provided transport for DARTWIC Share frames.
     *
     * DARTWICShare continues to own channel/event synchronization and routing;
     * the factory only creates the network link used by a configured connection.
     */
    struct ShareTypeDefinition {
        std::string id;
        std::string name;
        nlohmann::json default_config = nlohmann::json::object();
        std::function<ShareTransportPtr(const nlohmann::json& config)> create;
    };

    /**
     * Lifecycle callbacks and optional target frequency for a plugin-owned loop.
     *
     * The loop is controlled by CAESAR and stops with the engine.
     *
     * @dartwic-reference
     * @category Tasks and Loops
     * @example dartwic->registerLoop("heartbeat", "Heartbeat", {
     *     .on_loop = []() { publishHeartbeat(); },
     *     .target_frequency_hz = 10.0,
     * });
     */
    struct PluginLoopDefinition {
        std::function<void()> on_start;
        std::function<void()> on_loop;
        std::function<void()> on_end;
        std::optional<double> target_frequency_hz;
    };

    /**
     * Host API available to engine plugins and their module instances.
     *
     * Registration functions qualify local identifiers as `<plugin-id>.<local-id>`.
     * Channel authority calls require an active task or plugin loop controller.
     *
     * @dartwic-reference
     * @category Lifecycle
     */
    class SDK_API {
    public:
        SDK_API() = default;
        virtual ~SDK_API() = default;

        /**
         * Reads a typed field and returns the supplied default when the field is unavailable.
         * @dartwic-reference
         * @category Channels
         * @param channel Flat channel name.
         * @param field Field to read.
         * @param default_value Value used when the field is unavailable.
         * @returns The numeric field value.
         */
        virtual double queryChannelField(const std::string& channel,
            ChannelField field,
            std::optional<ChannelValue> default_value) = 0;

        /**
         * Inserts a channel field and fails when the addressed channel or field already exists.
         * @dartwic-reference
         * @category Channels
         * @param channel Flat channel name.
         * @param field Field to insert.
         * @param value Initial field value.
         */
        virtual void insertChannelField(const std::string& channel,
            ChannelField field,
            ChannelValue value) = 0;

        /**
         * Creates or replaces a field on a RAPID channel.
         * @dartwic-reference
         * @category Channels
         * @param channel Flat channel name.
         * @param field Field to write.
         * @param value New field value.
         */
        virtual void upsertChannelField(const std::string& channel,
            ChannelField field,
            ChannelValue value) = 0;

        /**
         * Removes a channel and its associated field data.
         * @dartwic-reference
         * @category Channels
         * @param channel Channel to remove.
         * @returns Whether a channel was removed.
         */
        virtual bool removeChannel(const std::string& channel) = 0;

        /**
         * Registers a plugin-local module type and returns its qualified identifier.
         * @dartwic-reference
         * @category Modules
         * @param definition Module type metadata and file paths.
         * @returns The plugin-qualified module type identifier.
         */
        virtual std::string registerModuleType(ModuleTypeDefinition definition) = 0;
        virtual std::string registerShareType(ShareTypeDefinition definition) = 0;
        /**
         * Registers a plugin-local task type and returns its qualified identifier.
         * @dartwic-reference
         * @category Tasks and Loops
         * @param local_id Identifier unique within the plugin.
         * @param name Operator-facing task type name.
         * @param definition Task metadata and lifecycle callbacks.
         * @returns The plugin-qualified task type identifier.
         */
        virtual std::string registerTaskType(std::string local_id, std::string name, TaskTypeDefinition definition) = 0;
        /**
         * Registers a plugin extension operation; it does not become an official DARTWIC operation.
         * @dartwic-reference
         * @category Operations
         * @param local_id Identifier unique within the plugin.
         * @param name Operator-facing operation name.
         * @param handler JSON request handler.
         * @returns The plugin-qualified operation identifier.
         */
        virtual std::string registerOperation(std::string local_id, std::string name, OperationHandler handler) = 0;
        /**
         * Registers a native DCode function together with its editor-facing argument documentation.
         * @dartwic-reference
         * @category DCode
         * @param local_id Identifier unique within the plugin.
         * @param name Operator-facing function name.
         * @param handler JSON function callback.
         * @param doc Function documentation shown by DCode tooling.
         * @param input_arguments Structured input documentation.
         * @param output_arguments Structured output documentation.
         * @returns The plugin-qualified DCode function identifier.
         */
        virtual std::string registerDCodeFunction(
            std::string local_id,
            std::string name,
            DCodeFunctionHandler handler,
            std::string doc = {},
            std::vector<DCodeFunctionArgument> input_arguments = {},
            std::vector<DCodeFunctionArgument> output_arguments = {}
        ) = 0;
        /**
         * Registers a CAESAR-controlled plugin loop and returns its qualified identifier.
         * @dartwic-reference
         * @category Tasks and Loops
         * @param local_id Identifier unique within the plugin.
         * @param name Operator-facing loop name.
         * @param definition Loop lifecycle and target frequency.
         * @returns The plugin-qualified loop identifier.
         */
        virtual std::string registerLoop(std::string local_id, std::string name, PluginLoopDefinition definition) = 0;

        /**
         * Raises an operator-visible error and returns the created event identifier.
         * @dartwic-reference
         * @category Events
         * @param message_title Short error title.
         * @param message_description Detailed error description.
         * @param tags Searchable event tags.
         * @param resolution Suggested operator resolution.
         * @param auto_acknowledge Non-zero to acknowledge automatically.
         * @returns The created ARGUS event identifier.
         */
        virtual int consoleError(
            std::string message_title,
            std::string message_description,
            std::vector<std::string> tags,
            std::string resolution,
            int auto_acknowledge = 0
        ) = 0;

        /**
         * Returns a live module instance by configured instance name.
         * @dartwic-reference
         * @category Modules
         * @param instance_name Configured module instance name.
         * @returns The live module instance, or null when it is unavailable.
         */
        virtual std::shared_ptr<Modules::BaseModule> getModuleInstance(const std::string& instance_name) = 0;
        /**
         * Lists live module instances, optionally limited to one plugin.
         * @dartwic-reference
         * @category Modules
         * @param plugin_id Optional canonical plugin identifier.
         * @returns Matching live module summaries.
         */
        virtual std::vector<ModuleInstanceSummary> getModuleInstances(const std::string& plugin_id = "") = 0;

        /**
         * Writes timestamped numeric samples to one channel in a single call.
         * @dartwic-reference
         * @category Channels
         * @param channel Channel receiving the samples.
         * @param data Value and Unix-nanosecond timestamp pairs.
         */
        virtual void upsertChannelValueBulk(const std::string& channel,
            const std::vector<std::pair<double, uint64_t>>& data) = 0;

        /**
         * Calls a registered native DCode function with a JSON payload.
         * @dartwic-reference
         * @category DCode
         * @param function_name Qualified or built-in function name.
         * @param payload Function arguments encoded as JSON.
         * @returns Function result encoded as JSON.
         */
        virtual nlohmann::json callDCodeFunction(const std::string& function_name, const nlohmann::json& payload) = 0;

        /**
         * Commands a channel while using the active task or loop as controller.
         * @dartwic-reference
         * @category Channel Authority
         * @param channel Flat channel name.
         * @param value Commanded value.
         */
        virtual void commandChannel(const std::string& channel, ChannelValue value) = 0;
        /**
         * Sets the active controller's value without issuing a manual command.
         * @dartwic-reference
         * @category Channel Authority
         * @param channel Flat channel name.
         * @param value Optional value; null clears the controller value.
         */
        virtual void setChannel(const std::string& channel, std::optional<ChannelValue> value = std::nullopt) = 0;
        /**
         * Claims channel authority for the active task or loop controller.
         * @dartwic-reference
         * @category Channel Authority
         * @param channel Flat channel name.
         */
        virtual void claimChannel(const std::string& channel) = 0;
        /**
         * Releases authority previously claimed by the active controller.
         * @dartwic-reference
         * @category Channel Authority
         * @param channel Flat channel name.
         */
        virtual void freeChannel(const std::string& channel) = 0;
    };
}

#endif //SDK_API_H
