# UHDM top Makefile (Wrapper to cmake)
PREFIX?=/usr/local

release: cmake-prepare
	$(MAKE) -C build

debug:
	mkdir -p build
	cd build; cmake ../ -DCMAKE_BUILD_TYPE=Debug
	$(MAKE) -C build

test: cmake-prepare
	$(MAKE) -C build test

clean:
	rm -f src/*
	rm -rf headers/*
	rm -rf build

install: cmake-prepare
	$(MAKE) -C build install

uninstall:
	rm -rf $(PREFIX)/lib/uhdm
	rm -rf $(PREFIX)/include/uhdm

cmake-prepare:
	mkdir -p build
	cd build; cmake ../ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX)

test_install:
	$(CXX) -std=c++14 -g tests/test1.cpp -I$(PREFIX)/include/uhdm -I$(PREFIX)/include/uhdm/include $(PREFIX)/lib/uhdm/libuhdm.a -lcapnp -lkj -ldl -lutil -lm -lrt -lpthread -o test_inst
	./test_inst
