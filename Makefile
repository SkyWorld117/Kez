ifndef FROMAGER_WORKDIR
$(error FROMAGER_WORKDIR is not set; source setup-env.sh first)
endif

FROMAGER_SYSTEM ?= $(FROMAGER_WORKDIR)/env/system

ifeq ($(origin CXX), default)
CXX := $(FROMAGER_SYSTEM)/bin/g++
endif
ifeq ($(origin AR), default)
AR := $(FROMAGER_SYSTEM)/bin/ar
endif

CPPFLAGS := -Iinclude -I$(FROMAGER_SYSTEM)/include -DFROMAGER_SOURCE_DIR=\"$(CURDIR)\"
CXXFLAGS ?= -O3
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS := -L$(FROMAGER_SYSTEM)/lib -L$(FROMAGER_SYSTEM)/lib64 \
	-Wl,-rpath,$(FROMAGER_SYSTEM)/lib -Wl,-rpath,$(FROMAGER_SYSTEM)/lib64
LDLIBS := -lyaml-cpp
TEST_LDLIBS := -lgtest -lgtest_main -pthread

SRC_DIR := src
OBJ_DIR := obj
LIB_DIR := lib
BIN_DIR := bin

LIB_SOURCES := \
	$(SRC_DIR)/database/build_parser.cpp \
	$(SRC_DIR)/database/config.cpp \
	$(SRC_DIR)/database/config_parser.cpp \
	$(SRC_DIR)/database/config_selector.cpp \
	$(SRC_DIR)/database/condition_parser.cpp \
	$(SRC_DIR)/database/database.cpp \
	$(SRC_DIR)/database/errors.cpp \
	$(SRC_DIR)/database/parser_utils.cpp \
	$(SRC_DIR)/database/source_parser.cpp \
	$(SRC_DIR)/utils/bash_utils.cpp \
	$(SRC_DIR)/utils/file_utils.cpp \
	$(SRC_DIR)/utils/string_utils.cpp
LIB_OBJECTS := $(LIB_SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
TEST_OBJECT := $(OBJ_DIR)/tests/database_test.o
LIBRARY := $(LIB_DIR)/libfromager.a
TEST_BINARY := $(BIN_DIR)/test_database

.DEFAULT_GOAL := all

.PHONY: all test clean

all: $(LIBRARY)

test: $(TEST_BINARY)
	$(TEST_BINARY)

$(LIBRARY): $(LIB_OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(TEST_BINARY): $(TEST_OBJECT) $(LIBRARY) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) $(TEST_LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(LIB_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(LIBRARY) $(TEST_BINARY)

-include $(LIB_OBJECTS:.o=.d) $(TEST_OBJECT:.o=.d)
