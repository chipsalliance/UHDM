# UHDM top Makefile (Wrapper to cmake)

# Use bash as the default shell
SHELL := /bin/bash

ifeq ($(CPU_CORES),)
	CPU_CORES := $(shell nproc)
	ifeq ($(CPU_CORES),)
		CPU_CORES := $(shell sysctl -n hw.physicalcpu)
	endif
	ifeq ($(CPU_CORES),)
		CPU_CORES := 2
	endif
endif

PREFIX?=/usr/local
ADDITIONAL_CMAKE_OPTIONS ?=

release: build
	cmake --build build --config Release -j $(CPU_CORES)

debug:
	mkdir -p dbuild
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild
	cmake --build dbuild --config Debug -j $(CPU_CORES)

test: build
	cmake --build build --target UnitTests --config Release -j $(CPU_CORES)
	cd build && ctest -C Release --output-on-failure

test-junit: release
	cd build && ctest --no-compress-output -T Test -C RelWithDebInfo --output-on-failure
	xsltproc .github/kokoro/ctest2junit.xsl build/Testing/*/Test.xml > build/test_results.xml

clean:
	rm -rf build
	rm -rf src/ headers/  # legacy location, not used anymore.

install: build
	cmake --install build --config Release

uninstall:
	$(RM) $(PREFIX)/bin/uhdm-dump $(PREFIX)/bin/uhdm-dump
	$(RM) -r $(PREFIX)/lib/uhdm
	$(RM) -r $(PREFIX)/include/uhdm

build:
	mkdir -p build
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

test_install:
	cmake --build build --target test_inst --config Release -j $(CPU_CORES)
	find build/bin -name test_inst* -exec {} \;
