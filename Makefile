CXXFLAGS ?= -O3
CXXFLAGS += -I$(CHEESE_ENV)/system/include
CXXFLAGS += -L$(CHEESE_ENV)/system/lib
CXXFLAGS += -L$(CHEESE_ENV)/system/lib64
CXXFLAGS += -lyaml-cpp
CXXFLAGS += -Wl,-rpath=$(CHEESE_ENV)/system/lib
CXXFLAGS += -Wl,-rpath=$(CHEESE_ENV)/system/lib64

cheese_config_verifier: src/package_format_verifier/*.cpp src/package_format_verifier/*.h
	g++ src/package_format_verifier/*.cpp -o bin/cheese_config_verifier $(CXXFLAGS)

cheese_deps_resolve: src/deps_resolve.h src/deps_resolve.cpp
	g++ -c src/deps_resolve.cpp -o src/deps_resolve.o $(CXXFLAGS)

test_deps_resolve: src/test_deps_resolve.cpp src/dependency_resolver/*.cpp
	g++ src/test_deps_resolve.cpp src/dependency_resolver/*.cpp -o bin/test_deps_resolve $(CXXFLAGS)

cheese_user_config_gen: src/user_config_gen.cpp src/dependency_resolver/*.cpp
	g++ src/user_config_gen.cpp src/dependency_resolver/*.cpp -o bin/cheese_user_config_gen $(CXXFLAGS)

.PHONY: all
all: cheese_config_verifier cheese_deps_resolve test_deps_resolve cheese_user_config_gen

.PHONY: release
release: cheese_config_verifier cheese_deps_resolve cheese_user_config_gen

.PHONY: clean
clean:
	rm -f bin/cheese_config_verifier
	rm -f bin/test_deps_resolve
	rm -f bin/cheese_user_config_gen
	rm -f src/*.o
