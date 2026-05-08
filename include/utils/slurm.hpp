#pragma once

#include <string>

inline std::string wrap_command_with_sbatch(const std::string& command,
                                            const std::string& job_name,
                                            const std::string& time = "01:00:00") {
    return "sbatch --nodes=1 --ntasks=1 --cpus-per-task=${FROMAGER_NPROC}"
           " --time=" + time +
           " --job-name=" + job_name +
           " --wrap=\"" + command + "\"";
}