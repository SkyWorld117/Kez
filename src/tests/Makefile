# Unit test rules
$(OBJ_DIR)/tests/test_string_utils.o: $(SRC_DIR)/tests/test_string_utils.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_toposort.o: $(SRC_DIR)/tests/test_toposort.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_database.o: $(SRC_DIR)/tests/test_database.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_cmdline_parser.o: $(SRC_DIR)/tests/test_cmdline_parser.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_package_format_verifier.o: $(SRC_DIR)/tests/test_package_format_verifier.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_parser.o: $(SRC_DIR)/tests/test_parser.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_rt_dependency_resolver.o: $(SRC_DIR)/tests/test_rt_dependency_resolver.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_rt_profile_config_parser.o: $(SRC_DIR)/tests/test_rt_profile_config_parser.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_ui.o: $(SRC_DIR)/tests/test_ui.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_user_config_generator.o: $(SRC_DIR)/tests/test_user_config_generator.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/tests/test_global_config.o: $(SRC_DIR)/tests/test_global_config.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/test_string_utils: $(OBJ_DIR)/tests/test_string_utils.o $(OBJ_DIR)/utils/string_utils.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_toposort: $(OBJ_DIR)/tests/test_toposort.o $(OBJ_DIR)/dependency_resolver/toposort.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_database: $(OBJ_DIR)/tests/test_database.o $(DATABASE_OBJS) $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_cmdline_parser: $(OBJ_DIR)/tests/test_cmdline_parser.o $(CMDLINE_PARSER_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_package_format_verifier: $(OBJ_DIR)/tests/test_package_format_verifier.o $(PACKAGE_FORMAT_VERIFIER_LIB_OBJS) $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_parser: $(OBJ_DIR)/tests/test_parser.o $(PARSER_OBJS) $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_rt_dependency_resolver: $(OBJ_DIR)/tests/test_rt_dependency_resolver.o $(RT_DEPENDENCY_RESOLVER_LIB_OBJS) $(DATABASE_OBJS) $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_rt_profile_config_parser: $(OBJ_DIR)/tests/test_rt_profile_config_parser.o $(RT_PROFILE_PARSER_LIB_OBJS) $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_ui: $(OBJ_DIR)/tests/test_ui.o $(UI_ARGPARSER_OBJS) $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_user_config_generator: $(OBJ_DIR)/tests/test_user_config_generator.o $(USER_CONFIG_GENERATOR_OBJS) $(DATABASE_OBJS) $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_global_config: $(OBJ_DIR)/tests/test_global_config.o $(GLOBAL_CONFIG_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)


.PHONY: test_cpp

TEST_BINS = \
	$(BIN_DIR)/test_string_utils \
	$(BIN_DIR)/test_toposort \
	$(BIN_DIR)/test_database \
	$(BIN_DIR)/test_cmdline_parser \
	$(BIN_DIR)/test_package_format_verifier \
	$(BIN_DIR)/test_parser \
	$(BIN_DIR)/test_rt_dependency_resolver \
	$(BIN_DIR)/test_rt_profile_config_parser \
	$(BIN_DIR)/test_ui \
	$(BIN_DIR)/test_user_config_generator \
	$(BIN_DIR)/test_global_config

test_cpp: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		$$test_bin || exit 1; \
	done

test: test_cpp
	@echo "Cleaning up test binaries..."
	rm -f $(TEST_BINS)