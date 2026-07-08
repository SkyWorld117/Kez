#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <variant>
#include <vector>

enum class PackageType {
    Package,
    System,
    Compiler,
    Mpi,
    Vendor,
    Abstract,
    External,
};

enum class Toolchain {
    None,
    Autotools,
    CMake,
    Make,
};

enum class SourceType {
    Git,
    Tarball,
    Zip,
    Script,
};

enum class ValueAction {
    Set,
    Append,
    Prepend,
};

template <typename T> struct ConditionalValue {
    std::string condition;
    ValueAction action = ValueAction::Set;
    T value;
};

template <typename T> struct ConfigurableValue {
    std::optional<T> default_value;
    std::vector<ConditionalValue<T>> conditions;
};

struct Release {
    std::string version;
    std::optional<std::string> url;
    std::optional<std::string> tag;
};

struct Source {
    SourceType type;
    std::optional<std::string> url;
    std::vector<Release> releases;
};

struct EnvironmentVariable {
    std::string name;
    std::optional<std::string> description;
    bool user_configurable = false;
    std::vector<std::string>
        requires;
    ConfigurableValue<std::string> value;
};

struct BuildOption {
    std::string name;
    std::optional<std::string> description;
    bool user_configurable = false;
    std::optional<ConfigurableValue<bool>> enabled;
    std::optional<std::string> enabled_format;
    std::optional<std::string> disabled_format;
    std::vector<std::string>
        requires;
    std::optional<ConfigurableValue<std::string>> enabled_value;
    std::optional<ConfigurableValue<std::string>> disabled_value;
};

struct BuildConfiguration {
    std::optional<std::string> command;
    std::vector<EnvironmentVariable> environment;
    std::vector<BuildOption> options;
};

struct BuildStage {
    std::optional<std::string> target;
    bool multithreaded = true;
    std::optional<BuildConfiguration> configurations;
};

struct Build {
    std::optional<std::string> preprocessing;
    std::optional<std::string> postprocessing;
    std::optional<BuildConfiguration> configurations;
    std::vector<BuildStage> stages;
};

struct Override {
    std::optional<std::string> condition;
    std::string target;
    ValueAction action = ValueAction::Set;
    std::string value;
};

using PropertyData = std::variant<std::string, ConfigurableValue<std::string>>;

struct Property {
    std::string name;
    PropertyData data;
};

class PackageConfig {
   public:
    virtual ~PackageConfig() = default;

    virtual Toolchain toolchain() const noexcept = 0;
    virtual std::optional<std::string> default_configuration_command() const;
    virtual std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                             unsigned int parallel_jobs) const;

    std::string name;
    std::optional<std::string> description;
    std::optional<std::string> author;
    PackageType type = PackageType::Package;
    std::optional<Source> source;
    std::vector<std::string> dependencies;
    std::vector<Override> overrides;
    std::optional<Build> build;
    std::vector<Property> properties;
    std::vector<std::string> implementations;
};

class GenericPackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
};

class AutotoolsPackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
    std::optional<std::string> default_configuration_command() const override;
    std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                     unsigned int parallel_jobs) const override;
};

class CMakePackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
    std::optional<std::string> default_configuration_command() const override;
    std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                     unsigned int parallel_jobs) const override;
};

class MakePackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
    std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                     unsigned int parallel_jobs) const override;
};

// Utility: find a property by name in a PackageConfig. Returns nullptr if not found.
inline const Property* find_property(const PackageConfig& config, const std::string& name) {
    const auto property =
        std::find_if(config.properties.begin(), config.properties.end(),
                     [&name](const Property& candidate) { return candidate.name == name; });
    return property == config.properties.end() ? nullptr : &*property;
}

// Utility: check if a PackageConfig has a property, with aliases for "includes" → "include"
// and "ldflags"/"nvldflags" → "lib".
inline bool has_property(const PackageConfig& config, const std::string& property) {
    if (find_property(config, property) != nullptr) {
        return true;
    }
    if (property == "includes") {
        return find_property(config, "include") != nullptr;
    }
    if (property == "ldflags" || property == "nvldflags") {
        return find_property(config, "lib") != nullptr;
    }
    return false;
}
