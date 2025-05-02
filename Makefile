UNAME_S := $(shell uname -s)

# On MacOS, the default compiler is clang
ifeq ($(UNAME_S), Darwin)
# Override CC if it is set to the default 'cc'
	ifeq ($(origin CC), default)
		CC := $(shell which clang)
	endif
	ifeq ($(origin CXX), default)
		CXX := $(shell which clang++)
	endif
else
	ifeq ($(origin CC), default)
		CC := $(shell which gcc)
	endif
	ifeq ($(origin CXX), default)
		CXX := $(shell which g++)
	endif
endif

ECHO := $(shell which echo) -e

RELEASE_DIR := build
DEBUG_DIR := debug


all: release
.PHONY: all

configure:
	@$(ECHO) "cmake -S . -B $(RELEASE_DIR) --log-level=NOTICE -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX)"
	@mkdir -p $(RELEASE_DIR)
	@cmake -S . -B $(RELEASE_DIR) \
	--log-level=NOTICE \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_COMPILER=$(CC) \
	-DCMAKE_CXX_COMPILER=$(CXX) \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5
.PHONY: configure

configure-debug:
	@mkdir -p $(DEBUG_DIR)
	@cmake -S . -B $(DEBUG_DIR) \
	--log-level=NOTICE \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_C_COMPILER=$(CC) \
	-DCMAKE_CXX_COMPILER=$(CXX)
.PHONY: configure-debug

release: configure
	@$(MAKE) -C ${RELEASE_DIR} qsyn
	@cp ${RELEASE_DIR}/qsyn .
	@$(ECHO) "Copied qsyn to $(shell pwd)"
.PHONY: release

debug: configure-debug
	@$(MAKE) -C ${DEBUG_DIR} qsyn-debug
	@cp ${DEBUG_DIR}/qsyn-debug .
	@$(ECHO) "Copied qsyn-debug to $(shell pwd)"
.PHONY: debug


# -----------------------------------------------------------------------------
# Testings, Linting and Cleaning
# -----------------------------------------------------------------------------

# run unit tests from the tests/src/ directory
unit-test: configure
	@$(MAKE) -C ${RELEASE_DIR} unit-test
	@$(ECHO) "Running unit tests..."
	@./${RELEASE_DIR}/qsyn-unit-test
.PHONY: unit-test

integrated-test: release
	@$(ECHO) "Running integrated tests..."
	@./scripts/RUN_TESTS

# run all tests with current qsyn binary at the root of the project
# use ./scripts/RUN_TESTS to run tests with specific dofiles
test: configure
	$(MAKE) -C ${RELEASE_DIR} qsyn unit-test
	@cp ${RELEASE_DIR}/qsyn .
	@$(ECHO) "Copied qsyn to $(shell pwd)"
	@$(ECHO) "Running unit tests..."
	@./${RELEASE_DIR}/qsyn-unit-test
	@$(ECHO) "Running integrated tests..."
	@./scripts/RUN_TESTS
.PHONY: test

test-update:
	./scripts/RUN_TESTS -u
.PHONY: test-update

# run clang-format and clang-tidy on the source code
lint:
	./scripts/LINT
.PHONY: lint

clean:
	rm -rf build
	rm -f qsyn
.PHONY: clean

clean-debug:
	rm -rf debug
	rm -f qsyn-debug
.PHONY: clean-debug

# -----------------------------------------------------------------------------
# Docker targets
# -----------------------------------------------------------------------------

# build the current source code in the docker container
build-docker:
	docker build -f docker/dev.Dockerfile -t qsyn-local .
.PHONY: build-docker

# run the binary you built with `make build-docker`
run-docker:
	docker run -it --rm -v $(shell pwd):/workdir qsyn-local 
.PHONY: run-docker

# compile and run all tests with current source code in the docker container
# use ./scripts/RUN_TESTS_DOCKER to run tests with specific dofiles
test-docker:
	./scripts/RUN_TESTS_DOCKER
.PHONY: test-docker

version := $(shell git describe --tags --abbrev=0)
publish:
	@$(ECHO) "publishing version $(version)"
	docker buildx build --push \
	--platform linux/amd64,linux/arm64 \
	-t dvlab/qsyn:$(version) \
	-f docker/prod.Dockerfile .
.PHONY: publish

clean-docker:
	docker container prune -f
	docker rmi qsyn-test-gcc -f
	docker rmi qsyn-test-clang -f
.PHONY: clean-docker
