UNAME_S := $(shell uname -s)

# Default target on Linux
ifeq ($(UNAME_S), Linux)
RELEASE_TARGET := build-g++
DEBUG_TARGET := debug-g++
endif

# Default target on macOS (Darwin)
ifeq ($(UNAME_S), Darwin)
RELEASE_TARGET := build-clang++
DEBUG_TARGET := debug-clang++
endif

all: build

build: $(RELEASE_TARGET)

build-g++:
	cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(shell which g++)
	$(MAKE) -C build

# force macos to use the clang++ installed by brew instead of the default one
# which is outdated
build-clang++:
	cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(shell which clang++)
	$(MAKE) -C build

# build the current source code in the docker container
build-docker:
	docker build -f docker/dev.Dockerfile -t qsyn-local .

debug: $(DEBUG_TARGET)

debug-g++:
	cmake -S . -B debug -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=$(shell which g++)
	$(MAKE) -C debug

# force macos to use the clang++ installed by brew instead of the default one
# which is outdated
debug-clang++:
	cmake -S . -B debug -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=$(shell which clang++)
	$(MAKE) -C debug

# run the binary you built with `make build-docker`
run-docker:
	docker run -it --rm -v $(shell pwd):/workdir qsyn-local 

# run all tests with current qsyn binary at the root of the project
# use ./scripts/RUN_TESTS to run tests with specific dofiles
test:
	./scripts/RUN_TESTS

# compile and run all tests with current source code in the docker container
# use ./scripts/RUN_TESTS_DOCKER to run tests with specific dofiles
test-docker:
	./scripts/RUN_TESTS_DOCKER

test-update:
	./scripts/RUN_TESTS -u

# run clang-format and clang-tidy on the source code
lint:
	./scripts/LINT

version := $(shell git describe --tags --abbrev=0)
publish:
	@echo "publishing version $(version)"
	docker buildx build --push --platform linux/amd64,linux/arm64 -t dvlab/qsyn:$(version) -f docker/prod.Dockerfile .

clean:
	rm -rf build
	rm -f qsyn

clean-debug:
	rm -rf debug
	rm -f qsyn-debug

clean-docker:
	docker container prune -f
	docker rmi qsyn-test-gcc -f
	docker rmi qsyn-test-clang -f

.PHONY: all build build-g++ build-clang++ debug debug-g++ debug-clang++ test test-docker test-update lint publish clean clean-debug clean-docker
