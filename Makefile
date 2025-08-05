CXXFLAGS ?= -O3
CXXFLAGS += -I$(FROMAGER_ENV)/system/include
CXXFLAGS += -L$(FROMAGER_ENV)/system/lib
CXXFLAGS += -L$(FROMAGER_ENV)/system/lib64
CXXFLAGS += -lyaml-cpp
CXXFLAGS += -Wl,-rpath=$(FROMAGER_ENV)/system/lib
CXXFLAGS += -Wl,-rpath=$(FROMAGER_ENV)/system/lib64

fromager_config_verifier: src/package_format_verifier/*.cpp src/package_format_verifier/*.h
	g++ src/package_format_verifier/*.cpp -o bin/fromager_config_verifier $(CXXFLAGS)

fromager_deps_resolve: src/deps_resolve.h src/deps_resolve.cpp
	g++ -c src/deps_resolve.cpp -o src/deps_resolve.o $(CXXFLAGS)

test_deps_resolve: src/test_deps_resolve.cpp src/dependency_resolver/*.cpp
	g++ src/test_deps_resolve.cpp src/dependency_resolver/*.cpp -o bin/test_deps_resolve $(CXXFLAGS)

fromager_user_config_gen: src/user_config_gen.cpp src/dependency_resolver/*.cpp
	g++ src/user_config_gen.cpp src/dependency_resolver/*.cpp -o bin/fromager_user_config_gen $(CXXFLAGS)

fromager_parser: src/parser/*.cpp src/parser/*.h
	g++ src/parser/*.cpp -o bin/fromager_parser $(CXXFLAGS)

.PHONY: all
all: fromager_config_verifier fromager_deps_resolve test_deps_resolve fromager_user_config_gen fromager_parser

.PHONY: release
release: fromager_config_verifier fromager_deps_resolve fromager_user_config_gen fromager_parser

.PHONY: clean
clean:
	rm -f bin/fromager_config_verifier
	rm -f bin/test_deps_resolve
	rm -f bin/fromager_user_config_gen
	rm -f bin/fromager_parser