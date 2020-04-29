# UHDM top Makefile (Wrapper to cmake)
PREFIX?=/usr/local

release: build
	$(MAKE) -C build

debug:
	mkdir -p build
	cd build; cmake ../ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX)
	$(MAKE) -C build

test: build
	$(MAKE) -C build test

clean:
	rm -f src/*
	rm -rf headers/*
	rm -rf build

install: build
	mkdir -p $(PREFIX)
	$(MAKE) -C build install

uninstall:
	rm -rf $(PREFIX)/lib/uhdm
	rm -rf $(PREFIX)/include/uhdm

build:
	mkdir -p build
	cd build; cmake ../ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX)

# TODO: the following static libraries should probably be all distributed together in libuhdm.a (ar ADDLIB)
test_install:
	$(CXX) -std=c++14 -g tests/test1.cpp -I$(PREFIX)/include/uhdm -I$(PREFIX)/include/uhdm/include $(PREFIX)/lib/uhdm/libuhdm.a $(PREFIX)/lib/uhdm/libcapnp.a $(PREFIX)/lib/uhdm/libkj.a -ldl -lutil -lm -lrt -lpthread -o test_inst
	./test_inst
