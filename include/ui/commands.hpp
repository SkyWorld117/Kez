#pragma once

#include <string>
#include <vector>

using CommandArguments = std::vector<std::string>;

void execute_init(const CommandArguments& arguments);
void execute_update(const CommandArguments& arguments);
void execute_install(const CommandArguments& arguments);
void execute_utilities(const CommandArguments& arguments);
void execute_template(const CommandArguments& arguments);
void execute_environment(const CommandArguments& arguments);
void execute_compiler(const CommandArguments& arguments);
void execute_mpi(const CommandArguments& arguments);
void execute_info(const CommandArguments& arguments);
void execute_selfcheck(const CommandArguments& arguments);
