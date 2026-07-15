#pragma once

#include <string>
#include <ui/commands.hpp>
#include <vector>

/** @brief Top-level command selected from the process argument list. */
enum class UiCommand {
    Help,
    Version,
    Init,
    Update,
    Install,
    Utilities,
    Uconf,
    Environment,
    Compiler,
    Mpi,
    Factory,
    Info,
    DbCheck,
};

/** @brief Non-terminating result of top-level command parsing. */
struct UiArgumentsParseResult {
    UiCommand command = UiCommand::Help;
    CommandArguments arguments;
    std::string error;
};

/** @brief Parse all process arguments following the executable name. */
UiArgumentsParseResult parse_ui_arguments(const CommandArguments& arguments);

/** @brief Options accepted by @c kez @c init. */
struct InitOptions {
    bool refresh             = false;
    bool use_distro_compiler = false;
};

/** @brief Non-terminating result of init option parsing. */
struct InitOptionsParseResult {
    InitOptions options;
    bool help = false;
    std::string error;
};

/** @brief Parse @c init options without printing or exiting. */
InitOptionsParseResult parse_init_options(const CommandArguments& arguments);

/** @brief Options accepted by @c kez @c update. */
struct UpdateOptions {
    bool with_system = false;
};

/** @brief Non-terminating result of update option parsing. */
struct UpdateOptionsParseResult {
    UpdateOptions options;
    bool help = false;
    std::string error;
};

/** @brief Parse @c update options without printing or exiting. */
UpdateOptionsParseResult parse_update_options(const CommandArguments& arguments);

/** @brief Parsed options shared by application and utility installation. */
struct InstallOptions {
    bool read_file  = false;              ///< Treat the positional value as a YAML file.
    bool dry_run    = false;              ///< Print the command plan without executing it.
    bool force      = false;              ///< Reinstall packages already present in state.
    bool with_slurm = false;              ///< Submit the executor through Slurm.
    bool rebuild    = false;              ///< Rebuild a package and its dependents.
    bool silent     = false;              ///< Generate configuration without terminal prompts.
    std::string environment;              ///< Target application environment.
    std::string renamed_version;          ///< Version label used only in the install prefix.
    std::string rebuild_package;          ///< Package selected by @c --rebuild.
    std::vector<std::string> overrides;   ///< Command-line YAML overrides.
    std::vector<std::string> positional;  ///< Package names or YAML path.
};

/** @brief Non-terminating result of install option parsing. */
struct InstallOptionsParseResult {
    InstallOptions options;
    bool help = false;
    std::string error;
};

/** @brief Parse install or utility-add options without printing or exiting. */
InstallOptionsParseResult parse_install_options(const CommandArguments& arguments, bool utility);

/** @brief Action selected for the shared utilities environment. */
enum class UtilitiesAction { Help, Add, Remove, Empty };

/** @brief Parsed utility action and its action-specific values. */
struct UtilitiesArguments {
    UtilitiesAction action = UtilitiesAction::Help;
    std::string package;
    CommandArguments install_arguments;
};

/** @brief Non-terminating result of utilities argument parsing. */
struct UtilitiesArgumentsParseResult {
    UtilitiesArguments arguments;
    std::string error;
};

/** @brief Parse @c utilities arguments without printing or exiting. */
UtilitiesArgumentsParseResult parse_utilities_arguments(const CommandArguments& arguments);

/** @brief Parsed arguments for the @c uconf command. */
struct UconfOptions {
    std::vector<std::string> packages;
    std::string output_path;
    bool silent = false;
};

/** @brief Non-terminating result of @c uconf option parsing. */
struct UconfOptionsParseResult {
    UconfOptions options;
    bool help = false;
    std::string error;
};

/** @brief Parse package names and output options for @c uconf. */
UconfOptionsParseResult parse_uconf_options(const CommandArguments& arguments);

/** @brief Parsed arguments for the @c info command. */
struct InfoArguments {
    std::string package;
    bool raw = false;
};

/** @brief Non-terminating result of @c info argument parsing. */
struct InfoArgumentsParseResult {
    InfoArguments arguments;
    bool help = false;
    std::string error;
};

/** @brief Parse a package name and optional @c --raw flag for @c info. */
InfoArgumentsParseResult parse_info_arguments(const CommandArguments& arguments);

/** @brief Parsed package selection for the @c dbcheck command. */
struct DbCheckOptionsParseResult {
    bool all_packages = false;
    bool help         = false;
    std::vector<std::string> packages;
    std::string error;
};

/** @brief Parse an optional @c --only package selection for @c dbcheck. */
DbCheckOptionsParseResult parse_dbcheck_options(const CommandArguments& arguments);

/** @brief Supported application-environment actions. */
enum class EnvironmentAction { Help, Create, Remove, List, Activate, Deactivate, Which, Empty };

/** @brief Parsed application-environment action and optional name. */
struct EnvironmentArguments {
    EnvironmentAction action = EnvironmentAction::Help;
    std::string name;
};

/** @brief Non-terminating result of application-environment parsing. */
struct EnvironmentArgumentsParseResult {
    EnvironmentArguments arguments;
    std::string error;
};

/** @brief Parse @c env arguments without printing or exiting. */
EnvironmentArgumentsParseResult parse_environment_arguments(const CommandArguments& arguments);

/** @brief Supported compiler and MPI environment actions. */
enum class ManagedEnvironmentAction { Help, Load, Unload, List, Which, Remove };

/** @brief Parsed compiler or MPI action and optional name. */
struct ManagedEnvironmentArguments {
    ManagedEnvironmentAction action = ManagedEnvironmentAction::Help;
    std::string name;
};

/** @brief Non-terminating result of compiler or MPI argument parsing. */
struct ManagedEnvironmentArgumentsParseResult {
    ManagedEnvironmentArguments arguments;
    std::string error;
};

/** @brief Parse compiler or MPI arguments using @p command in diagnostics. */
ManagedEnvironmentArgumentsParseResult parse_managed_environment_arguments(
    const CommandArguments& arguments, const std::string& command);

/** @brief Options controlling factory-build plan execution. */
struct FactoryBuildOptions {
    bool dry_run    = false;
    bool force      = false;
    bool with_slurm = false;
};

/** @brief Supported factory actions. */
enum class FactoryAction { Help, Create, Remove, List, Enter, Exit, Which, Build, Run, Summarize };

/** @brief Parsed factory action and action-specific values. */
struct FactoryArguments {
    FactoryAction action = FactoryAction::Help;
    std::string name;
    FactoryBuildOptions build_options;
};

/** @brief Non-terminating result of factory argument parsing. */
struct FactoryArgumentsParseResult {
    FactoryArguments arguments;
    std::string error;
};

/** @brief Parse factory actions and build options without printing or exiting. */
FactoryArgumentsParseResult parse_factory_arguments(const CommandArguments& arguments);
