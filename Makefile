SHELL := /bin/sh
BUILD_TYPE ?= Release
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

.PHONY: build debug test example clean phpize-build

build:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j$(JOBS)

debug:
	cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
	cmake --build build-debug -j$(JOBS)

test: build
	PHPSHIELD_EXT="build/modules/phpshield.so" "tests/run.sh"

example: build
	bin/phpshield init
	bin/phpshield encode examples/simple tmp/example --key-file tmp/master.key
	php -d extension=build/modules/phpshield.so -d phpshield.bundle=tmp/example/app.pshield -d phpshield.key="$$(cat tmp/master.key)" tmp/example/index.php

phpize-build:
	cd loader && phpize && ./configure && make

clean:
	rm -rf build build-debug tmp tests/out tests/tmp
