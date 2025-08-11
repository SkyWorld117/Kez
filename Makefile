# Compiler and flags
CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17
CXXFLAGS += -I$(FROMAGER_ENV)/system/include
LDFLAGS += -L$(FROMAGER_ENV)/system/lib -L$(FROMAGER_ENV)/system/lib64
LDFLAGS += -lyaml-cpp
LDFLAGS += -Wl,-rpath=$(FROMAGER_ENV)/system/lib -Wl,-rpath=$(FROMAGER_ENV)/system/lib64

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Object files for different components
PACKAGE_FORMAT_VERIFIER_OBJS = $(OBJ_DIR)/build_verifier.o $(OBJ_DIR)/cheese_verifier.o \
                               $(OBJ_DIR)/conditions_verifier.o $(OBJ_DIR)/configurations_verifier.o \
                               $(OBJ_DIR)/dependencies_verifier.o $(OBJ_DIR)/environment_verifier.o \
                               $(OBJ_DIR)/implementations_verifier.o $(OBJ_DIR)/options_verifier.o \
                               $(OBJ_DIR)/properties_verifier.o $(OBJ_DIR)/source_verifier.o \
                               $(OBJ_DIR)/stages_verifier.o $(OBJ_DIR)/templates_verifier.o \
                               $(OBJ_DIR)/main.o

DEPENDENCY_RESOLVER_OBJS = $(OBJ_DIR)/essential_dependencies.o $(OBJ_DIR)/optional_dependencies.o \
                          $(OBJ_DIR)/resolve_dependencies.o $(OBJ_DIR)/toposort.o

PARSER_OBJS = $(OBJ_DIR)/conditions_parser.o $(OBJ_DIR)/configuration_parser.o \
              $(OBJ_DIR)/environment_parser.o $(OBJ_DIR)/filter.o \
              $(OBJ_DIR)/options_parser.o $(OBJ_DIR)/package_parser.o \
              $(OBJ_DIR)/parser.o $(OBJ_DIR)/property_parser.o \
              $(OBJ_DIR)/scalar_parser.o $(OBJ_DIR)/template_parser.o

COLORS_OBJS = $(OBJ_DIR)/colors_info.o $(OBJ_DIR)/colors_warning.o \
              $(OBJ_DIR)/colors_error.o $(OBJ_DIR)/colors_success.o

# Default target
.DEFAULT_GOAL := release

# Create object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Object file rules for package format verifier
$(OBJ_DIR)/%.o: $(SRC_DIR)/package_format_verifier/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Object file rules for dependency resolver
$(OBJ_DIR)/%.o: $(SRC_DIR)/dependency_resolver/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Object file rules for parser
$(OBJ_DIR)/%.o: $(SRC_DIR)/parser/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Object file rules for colors (with prefixed names to avoid conflicts)
$(OBJ_DIR)/colors_%.o: $(SRC_DIR)/colors/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Special rules for main executables
$(OBJ_DIR)/test_deps_resolve.o: $(SRC_DIR)/test_deps_resolve.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/user_config_gen.o: $(SRC_DIR)/user_config_gen.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Executables
$(BIN_DIR)/fromager_config_verifier: $(PACKAGE_FORMAT_VERIFIER_OBJS) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_deps_resolve: $(OBJ_DIR)/test_deps_resolve.o $(DEPENDENCY_RESOLVER_OBJS) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_user_config_gen: $(OBJ_DIR)/user_config_gen.o $(DEPENDENCY_RESOLVER_OBJS) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_parser: $(PARSER_OBJS) | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_info: $(OBJ_DIR)/colors_info.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_warning: $(OBJ_DIR)/colors_warning.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_error: $(OBJ_DIR)/colors_error.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/fromager_success: $(OBJ_DIR)/colors_success.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

# Create bin directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Phony targets
.PHONY: fromager_colored_io all release clean help

fromager_colored_io: $(BIN_DIR)/fromager_info $(BIN_DIR)/fromager_warning $(BIN_DIR)/fromager_error $(BIN_DIR)/fromager_success

all: $(BIN_DIR)/fromager_config_verifier $(BIN_DIR)/test_deps_resolve $(BIN_DIR)/fromager_user_config_gen $(BIN_DIR)/fromager_parser

release: $(BIN_DIR)/fromager_config_verifier $(BIN_DIR)/fromager_user_config_gen $(BIN_DIR)/fromager_parser fromager_colored_io

help:
	@echo "Available targets:"
	@echo "  release                     - Build release binaries (default)"
	@echo "  all                         - Build all binaries including test tools"
	@echo "  fromager_colored_io         - Build colored I/O utilities"
	@echo "  fromager_config_verifier    - Build config verifier"
	@echo "  fromager_user_config_gen    - Build user config generator"
	@echo "  fromager_parser             - Build parser"
	@echo "  test_deps_resolve           - Build dependency resolver test"
	@echo "  clean                       - Remove all built files"
	@echo "  help                        - Show this help message"

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BIN_DIR)/fromager_config_verifier
	rm -f $(BIN_DIR)/test_deps_resolve
	rm -f $(BIN_DIR)/fromager_user_config_gen
	rm -f $(BIN_DIR)/fromager_parser
	rm -f $(BIN_DIR)/fromager_info
	rm -f $(BIN_DIR)/fromager_warning
	rm -f $(BIN_DIR)/fromager_error
	rm -f $(BIN_DIR)/fromager_success