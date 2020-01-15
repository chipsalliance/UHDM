# UHDM top Makefile (Wrapper to cmake)

release:
	mkdir -p build
	cd build; cmake ../ -DCMAKE_BUILD_TYPE=Release
	$(MAKE) -C build

debug:
	mkdir -p build
	cd build; cmake ../ -DCMAKE_BUILD_TYPE=Debug
	$(MAKE) -C build

test:
	$(MAKE) -C build test

clean:
	rm -f src/*
	rm -rf headers/*
	rm -rf build

install:
	$(MAKE) -C build install

uninstall:
	rm -rf /usr/local/lib/uhdm
	rm -rf /usr/local/include/uhdm

test_install:
	$(CXX) -std=c++14 tests/test1.cpp -I/usr/local/include/uhdm -I/usr/local/include/uhdm/include /usr/local/lib/uhdm/libuhdm.a -lcapnp -lkj -ldl -lutil -lm -lrt -lpthread -o test_inst
	./test_inst
