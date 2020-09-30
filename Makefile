# UHDM top Makefile (Wrapper to cmake)
PREFIX?=/usr/local

release: build
	cmake --build build --config Release

debug:
	mkdir -p build
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) -S . -B build
	cmake --build build --config Debug

test: build
	cmake --build build --target UnitTests --config Release
	cd build && ctest -C Release --output-on-failure

test-junit: release
	cd build && ctest --no-compress-output -T Test -C RelWithDebInfo --output-on-failure
	xsltproc .github/kokoro/ctest2junit.xsl build/Testing/*/Test.xml > build/test_results.xml

clean:
	rm -f src/*
	rm -rf headers/*
	rm -rf build

install: build
	cmake --install build --config Release

uninstall:
	rm -rf $(PREFIX)/lib/uhdm
	rm -rf $(PREFIX)/include/uhdm

build:
	mkdir -p build
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -S . -B build

# TODO: the following static libraries should probably be all distributed together in libuhdm.a (ar ADDLIB)
test_install:
	cmake --build build --target test_inst --config Release
	find build/bin -name test_inst* -exec {} \;
