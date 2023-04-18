# UHDM top Makefile (Wrapper to cmake)

# Use bash as the default shell
SHELL := /usr/bin/env bash

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
export CTEST_PARALLEL_LEVEL = $(CPU_CORES)

release: build
	cmake --build build --config Release -j $(CPU_CORES)

release-shared: build-shared
	cmake --build build --config Release -j $(CPU_CORES)

debug:
	mkdir -p dbuild
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild
	cmake --build dbuild --config Debug -j $(CPU_CORES)

debug-shared:
	mkdir -p dbuild
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DBUILD_SHARED_LIBS=ON $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild
	cmake --build dbuild --config Debug -j $(CPU_CORES)


test: build
	cmake --build build --target UnitTests --config Release -j $(CPU_CORES)
	cd build && ctest -C Release --output-on-failure

test-shared: build-shared
	cmake --build build --target UnitTests --config Release -j $(CPU_CORES)
	cd build && ctest -C Release --output-on-failure


test-junit: release
	cd build && ctest --no-compress-output -T Test -C RelWithDebInfo --output-on-failure
	xsltproc .github/kokoro/ctest2junit.xsl build/Testing/*/Test.xml > build/test_results.xml

clean:
	rm -rf build dbuild

install: release
	cmake --install build --config Release

install-shared: release-shared
	cmake --install build --config Release

uninstall:
	$(RM) $(PREFIX)/bin/uhdm-dump $(PREFIX)/bin/uhdm-dump
	$(RM) -r $(PREFIX)/lib/uhdm
	$(RM) -r $(PREFIX)/include/uhdm

build:
	mkdir -p build
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DBUILD_SHARED_LIBS=OFF $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

build-shared:
	mkdir -p build
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DBUILD_SHARED_LIBS=ON $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

test_install:
	cmake --build build --target test_inst --config Release -j $(CPU_CORES)
	find build/bin -name test_inst* -exec {} \;

# Smoke-test if installation results in usable pkg-config
# Depending on system, PKG_CONFIG_PATH env var has different name. Set both.
test_install_pkgconfig: install
	PKG_CONFIG_PATH="$(PREFIX)/lib/pkgconfig:${PKG_CONFIG_PATH}" \
	PKG_CONFIG_PATH_FOR_TARGET="$(PREFIX)/lib/pkgconfig:${PKG_CONFIG_PATH_FOR_TARGET}" \
	PKG_CONFIG_CXXFLAGS=`pkg-config --cflags UHDM` && \
	PKG_CONFIG_LDFLAGS=`pkg-config --libs UHDM` && \
	echo -e "pkg-config:\n\tCXXFLAGS=$$PKG_CONFIG_CXXFLAGS\n\tLDFLAGS=$$PKG_CONFIG_LDFLAGS" && \
	$(CXX) --std=c++17 $$PKG_CONFIG_CXXFLAGS util/uhdm-dump.cpp -obuild/uhdm-dump_pkg-config_test-compile $$PKG_CONFIG_LDFLAGS

.PHONY: build build-shared build-static release debug test test-junit clean install uninstall test_install
