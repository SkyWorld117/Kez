ifndef KEZ_WORKDIR
$(error KEZ_WORKDIR is not set; source setup-env.sh first)
endif

KEZ_SYSTEM ?= $(KEZ_WORKDIR)/env/system

ifeq ($(origin CXX), default)
CXX := $(KEZ_SYSTEM)/bin/g++
endif
ifeq ($(origin AR), default)
AR := $(shell $(CXX) -print-prog-name=gcc-ar)
endif

CPPFLAGS := -Iinclude -I$(KEZ_SYSTEM)/include -DKEZ_SOURCE_DIR=\"$(CURDIR)\"
CXXFLAGS ?= -O3 -flto
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS := -L$(KEZ_SYSTEM)/lib -L$(KEZ_SYSTEM)/lib64 \
	-Wl,-rpath,$(KEZ_SYSTEM)/lib -Wl,-rpath,$(KEZ_SYSTEM)/lib64
LDLIBS := -lyaml-cpp
TEST_LDLIBS := -lgtest -lgtest_main -pthread

SRC_DIR := src
OBJ_DIR := obj
LIB_DIR := lib
BIN_DIR := bin

LIB_SOURCES := \
	$(SRC_DIR)/cmdline_parser/cmdline_parser.cpp \
	$(SRC_DIR)/cmdline_parser/traverse.cpp \
	$(SRC_DIR)/database/build_parser.cpp \
	$(SRC_DIR)/database/config.cpp \
	$(SRC_DIR)/database/config_parser.cpp \
	$(SRC_DIR)/database/config_selector.cpp \
	$(SRC_DIR)/database/condition_parser.cpp \
	$(SRC_DIR)/database/database.cpp \
	$(SRC_DIR)/database/parser_utils.cpp \
	$(SRC_DIR)/database/source_parser.cpp \
	$(SRC_DIR)/dependency_resolver/advisor.cpp \
	$(SRC_DIR)/dependency_resolver/essential_dependencies.cpp \
	$(SRC_DIR)/dependency_resolver/optional_dependencies.cpp \
	$(SRC_DIR)/dependency_resolver/requirements.cpp \
	$(SRC_DIR)/dependency_resolver/resolve_dependencies.cpp \
	$(SRC_DIR)/dependency_resolver/toposort.cpp \
	$(SRC_DIR)/parser/source_commands.cpp \
	$(SRC_DIR)/parser/template_resolver.cpp \
	$(SRC_DIR)/parser/user_config_parser.cpp \
	$(SRC_DIR)/user_config_generator/configurations_filter.cpp \
	$(SRC_DIR)/user_config_generator/environment_filter.cpp \
	$(SRC_DIR)/user_config_generator/options_filter.cpp \
	$(SRC_DIR)/user_config_generator/stages_filter.cpp \
	$(SRC_DIR)/user_config_generator/user_config_generator.cpp \
	$(SRC_DIR)/ui/bash_completion.cpp \
	$(SRC_DIR)/ui/ui_utils.cpp \
	$(SRC_DIR)/utils/bash_utils.cpp \
	$(SRC_DIR)/utils/file_utils.cpp \
	$(SRC_DIR)/utils/string_utils.cpp \
	$(SRC_DIR)/utils/yaml_utils.cpp
LIB_OBJECTS := $(LIB_SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
CLI_SOURCES := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/ui/environment.cpp \
	$(SRC_DIR)/ui/init.cpp \
	$(SRC_DIR)/ui/install.cpp \
	$(SRC_DIR)/ui/packages.cpp \
	$(SRC_DIR)/ui/ui.cpp
CLI_OBJECTS := $(CLI_SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
COMPLETION_SOURCE := $(SRC_DIR)/bash_completion_main.cpp
COMPLETION_OBJECT := $(OBJ_DIR)/bash_completion_main.o
TEST_SOURCES := \
	tests/bash_completion_test.cpp \
	tests/cmdline_parser_test.cpp \
	tests/database_test.cpp \
	tests/dependency_resolver_test.cpp \
	tests/utils_test.cpp \
	tests/user_config_parser_test.cpp \
	tests/user_config_generator_test.cpp
TEST_OBJECTS := $(TEST_SOURCES:%.cpp=$(OBJ_DIR)/%.o)
LIBRARY := $(LIB_DIR)/libkez.a
TEST_BINARY := $(BIN_DIR)/test_database
CLI_BINARY := $(BIN_DIR)/kez
COMPLETION_BINARY := $(BIN_DIR)/kez_completion

.DEFAULT_GOAL := all

.PHONY: all test clean

all: $(LIBRARY) $(CLI_BINARY) $(COMPLETION_BINARY)

test: $(TEST_BINARY)
	$(TEST_BINARY)

$(LIBRARY): $(LIB_OBJECTS) | $(LIB_DIR)
	$(RM) $@
	$(AR) rcs $@ $^

$(TEST_BINARY): $(TEST_OBJECTS) $(LIBRARY) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) $(TEST_LDLIBS) -o $@

$(CLI_BINARY): $(CLI_OBJECTS) $(LIBRARY) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(COMPLETION_BINARY): $(COMPLETION_OBJECT) $(LIBRARY) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(LIB_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(LIBRARY) $(TEST_BINARY) $(CLI_BINARY) $(COMPLETION_BINARY)

-include $(LIB_OBJECTS:.o=.d) $(CLI_OBJECTS:.o=.d) $(COMPLETION_OBJECT:.o=.d) \
	$(TEST_OBJECTS:.o=.d)
