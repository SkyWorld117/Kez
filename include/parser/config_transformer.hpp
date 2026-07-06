#pragma once

#include <database/config.hpp>
#include <string>

struct UserConfigParserContext;

namespace parser {

    std::string format_include_path(const std::string& path);
    std::string format_library_path(const std::string& path, UserConfigParserContext& context);
    std::string format_nvidia_library_path(const std::string& path);

    std::string transform_configuration(const BuildConfiguration& configuration,
                                        const PackageConfig& package, Toolchain toolchain,
                                        UserConfigParserContext& context);

}  // namespace parser
