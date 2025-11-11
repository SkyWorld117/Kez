# Compiler and flags
CXX ?= g++
CXXFLAGS ?= -O3 -flto -std=c++17
INCLUDES = -I$(FROMAGER_ENV)/system/include
LDFLAGS += -L$(FROMAGER_ENV)/system/lib -L$(FROMAGER_ENV)/system/lib64
LDFLAGS += -lyaml-cpp
LDFLAGS += -Wl,-rpath=$(FROMAGER_ENV)/system/lib -Wl,-rpath=$(FROMAGER_ENV)/system/lib64

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Object files for different components
PACKAGE_FORMAT_VERIFIER_OBJS = \
	$(OBJ_DIR)/package_format_verifier/build_verifier.o \
	$(OBJ_DIR)/package_format_verifier/cheese_verifier.o \
	$(OBJ_DIR)/package_format_verifier/conditions_verifier.o \
	$(OBJ_DIR)/package_format_verifier/configurations_verifier.o \
	$(OBJ_DIR)/package_format_verifier/dependencies_verifier.o \
	$(OBJ_DIR)/package_format_verifier/environment_verifier.o \
	$(OBJ_DIR)/package_format_verifier/implementations_verifier.o \
	$(OBJ_DIR)/package_format_verifier/options_verifier.o \
	$(OBJ_DIR)/package_format_verifier/properties_verifier.o \
	$(OBJ_DIR)/package_format_verifier/source_verifier.o \
	$(OBJ_DIR)/package_format_verifier/stages_verifier.o \
	$(OBJ_DIR)/package_format_verifier/templates_verifier.o \
	$(OBJ_DIR)/package_format_verifier/main.o

DEPENDENCY_RESOLVER_OBJS = \
	$(OBJ_DIR)/dependency_resolver/advisor.o \
	$(OBJ_DIR)/dependency_resolver/essential_dependencies.o \
	$(OBJ_DIR)/dependency_resolver/optional_dependencies.o \
	$(OBJ_DIR)/dependency_resolver/resolve_dependencies.o \
	$(OBJ_DIR)/dependency_resolver/toposort.o

USER_CONFIG_GENERATOR_OBJS = \
	$(OBJ_DIR)/user_config_generator/configurations_filter.o \
	$(OBJ_DIR)/user_config_generator/environment_filter.o \
	$(OBJ_DIR)/user_config_generator/options_filter.o \
	$(OBJ_DIR)/user_config_generator/stages_filter.o \
	$(OBJ_DIR)/user_config_generator/user_config_generator.o \
	$(OBJ_DIR)/user_config_generator/main.o

PARSER_OBJS = \
	$(OBJ_DIR)/parser/conditions_parser.o \
	$(OBJ_DIR)/parser/configuration_parser.o \
	$(OBJ_DIR)/parser/environment_parser.o \
	$(OBJ_DIR)/parser/filter.o \
	$(OBJ_DIR)/parser/options_parser.o \
	$(OBJ_DIR)/parser/package_parser.o \
	$(OBJ_DIR)/parser/parser.o \
	$(OBJ_DIR)/parser/property_parser.o \
	$(OBJ_DIR)/parser/scalar_parser.o \
	$(OBJ_DIR)/parser/template_parser.o \
	$(OBJ_DIR)/parser/main.o

CMDLINE_PARSER_OBJS = \
	$(OBJ_DIR)/cmdline_parser/traverse.o \
	$(OBJ_DIR)/cmdline_parser/main.o

RT_PROFILE_PARSER_OBJS = \
	$(OBJ_DIR)/rt_profile_config_parser/factory_parser.o \
	$(OBJ_DIR)/rt_profile_config_parser/cellar_parser.o \
	$(OBJ_DIR)/rt_profile_config_parser/run_config_parser.o \
	$(OBJ_DIR)/rt_profile_config_parser/resource_manager.o \
	$(OBJ_DIR)/rt_profile_config_parser/main.o

RT_DEPENDENCY_RESOLVER_OBJS = \
	$(OBJ_DIR)/rt_dependency_resolver/dependents.o \
	$(OBJ_DIR)/rt_dependency_resolver/unbuilt_dependencies.o \
	$(OBJ_DIR)/rt_dependency_resolver/main.o

DATABASE_OBJS = \
	$(OBJ_DIR)/database/database.o

# Library versions (without main.o files)
USER_CONFIG_GENERATOR_LIB_OBJS = $(filter-out $(OBJ_DIR)/user_config_generator/main.o, $(USER_CONFIG_GENERATOR_OBJS))
PARSER_LIB_OBJS = $(filter-out $(OBJ_DIR)/parser/main.o, $(PARSER_OBJS))
DEPENDENCY_RESOLVER_LIB_OBJS = $(filter-out $(OBJ_DIR)/dependency_resolver/main.o, $(DEPENDENCY_RESOLVER_OBJS))

# Default target
.DEFAULT_GOAL := release

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)/package_format_verifier
	mkdir -p $(OBJ_DIR)/dependency_resolver
	mkdir -p $(OBJ_DIR)/user_config_generator
	mkdir -p $(OBJ_DIR)/parser
	mkdir -p $(OBJ_DIR)/cmdline_parser
	mkdir -p $(OBJ_DIR)/rt_profile_config_parser
	mkdir -p $(OBJ_DIR)/rt_dependency_resolver
	mkdir -p $(OBJ_DIR)/colors
	mkdir -p $(OBJ_DIR)/database
	mkdir -p $(OBJ_DIR)/tests

# Object file rules for package format verifier
$(OBJ_DIR)/package_format_verifier/%.o: $(SRC_DIR)/package_format_verifier/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for dependency resolver
$(OBJ_DIR)/dependency_resolver/%.o: $(SRC_DIR)/dependency_resolver/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for user config generator
$(OBJ_DIR)/user_config_generator/%.o: $(SRC_DIR)/user_config_generator/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for parser
$(OBJ_DIR)/parser/%.o: $(SRC_DIR)/parser/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for cmdline parser
$(OBJ_DIR)/cmdline_parser/%.o: $(SRC_DIR)/cmdline_parser/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for rt profile config parser
$(OBJ_DIR)/rt_profile_config_parser/%.o: $(SRC_DIR)/rt_profile_config_parser/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for rt dependency resolver
$(OBJ_DIR)/rt_dependency_resolver/%.o: $(SRC_DIR)/rt_dependency_resolver/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for colors (with prefixed names to avoid conflicts)
$(OBJ_DIR)/colors/%.o: $(SRC_DIR)/colors/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Object file rules for database
$(OBJ_DIR)/database/%.o: $(SRC_DIR)/database/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Special rules for main executables
$(OBJ_DIR)/tests/test_deps_resolve.o: $(SRC_DIR)/tests/test_deps_resolve.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Executables
$(BIN_DIR)/fromager_config_verifier: $(PACKAGE_FORMAT_VERIFIER_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_deps_resolve: $(OBJ_DIR)/tests/test_deps_resolve.o $(DEPENDENCY_RESOLVER_OBJS) $(DATABASE_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_user_config_gen: $(USER_CONFIG_GENERATOR_OBJS) $(DEPENDENCY_RESOLVER_OBJS) $(DATABASE_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_parser: $(PARSER_OBJS) $(DATABASE_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_cmdline_parser: $(CMDLINE_PARSER_OBJS) $(USER_CONFIG_GENERATOR_LIB_OBJS) $(PARSER_LIB_OBJS) $(DEPENDENCY_RESOLVER_OBJS) $(DATABASE_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_rt_profile_config_parser: $(RT_PROFILE_PARSER_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_rt_resolve_dependencies: $(RT_DEPENDENCY_RESOLVER_OBJS) $(DEPENDENCY_RESOLVER_LIB_OBJS) $(DATABASE_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_%: $(OBJ_DIR)/colors/%.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Create bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Phony targets
.PHONY: fromager_colored_io all release clean help

fromager_colored_io: $(BIN_DIR)/fromager_info $(BIN_DIR)/fromager_warning $(BIN_DIR)/fromager_error $(BIN_DIR)/fromager_success

all: $(BIN_DIR)/fromager_config_verifier $(BIN_DIR)/test_deps_resolve $(BIN_DIR)/fromager_user_config_gen $(BIN_DIR)/fromager_parser $(BIN_DIR)/fromager_cmdline_parser $(BIN_DIR)/fromager_rt_profile_config_parser $(BIN_DIR)/fromager_rt_resolve_dependencies fromager_colored_io

release: $(BIN_DIR)/fromager_config_verifier $(BIN_DIR)/fromager_user_config_gen $(BIN_DIR)/fromager_parser $(BIN_DIR)/fromager_cmdline_parser $(BIN_DIR)/fromager_rt_profile_config_parser $(BIN_DIR)/fromager_rt_resolve_dependencies fromager_colored_io

help:
	@echo "Available targets:"
	@echo "  release                     - Build release binaries (default)"
	@echo "  all                         - Build all binaries including test tools"
	@echo "  clean                       - Remove all built files"
	@echo "  help                        - Show this help message"

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BIN_DIR)/fromager_config_verifier
	rm -f $(BIN_DIR)/fromager_user_config_gen
	rm -f $(BIN_DIR)/fromager_parser
	rm -f $(BIN_DIR)/fromager_cmdline_parser
	rm -f $(BIN_DIR)/fromager_rt_profile_config_parser
	rm -f $(BIN_DIR)/fromager_rt_resolve_dependencies
	rm -f $(BIN_DIR)/fromager_info
	rm -f $(BIN_DIR)/fromager_warning
	rm -f $(BIN_DIR)/fromager_error
	rm -f $(BIN_DIR)/fromager_success
	rm -f $(BIN_DIR)/test_deps_resolve