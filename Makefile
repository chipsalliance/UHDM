# UHDM top Makefile (Wrapper to cmake)

release:
	mkdir -p build
	cd build; cmake ../ -DCMAKE_BUILD_TYPE=Release
	$(MAKE) -C build

test:
	$(MAKE) -C build test

clean:
	$(MAKE) -C build clean
	rm -rf build

install:
	$(MAKE) -C build install

uninstall:
	rm -rf /usr/local/lib/uhdm
	rm -rf /usr/local/include/uhdm
