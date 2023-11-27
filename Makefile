all: build

build:
	cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
	cmake --build build --parallel 6

# force macos to use the clang++ installed by brew instead of the default one
# which is outdated
build-clang++:
	cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 $(shell which clang++)
	cmake --build build --parallel 6

# build the current source code in the docker container
build-docker:
	docker build -f docker/dev.Dockerfile -t qsyn-local .

# run the binary you built with `make build-docker`
run-docker:
	docker run -it --rm -v $(shell pwd):/workdir qsyn-local 

# run all tests with current qsyn binary at the root of the project
# use ./scripts/RUN_TESTS to run tests with specific dofiles
test:
	./scripts/RUN_TESTS ./tests

# compile and run all tests with current source code in the docker container
# use ./scripts/RUN_TESTS_DOCKER to run tests with specific dofiles
test-docker:
	./scripts/RUN_TESTS_DOCKER ./tests

version := $(shell git describe --tags --abbrev=0)
publish:
	@echo "publishing version $(version)"
	docker buildx build --push --platform linux/amd64,linux/arm64 -t dvlab/qsyn:$(version) -f docker/prod.Dockerfile .

clean:
	rm -rf build
	rm -f qsyn

.PHONY: all build build-clang++ test test-docker publish clean
