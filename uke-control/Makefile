SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
CXX ?= g++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++20 -Wall -Wextra -Werror -fPIC
CPPFLAGS += -Isrc -Ibuild $(shell pkg-config --cflags libdrm libsystemd wayland-client libselinux)
LDLIBS = $(shell pkg-config --libs libdrm libsystemd wayland-client)
outdir ?= $(CURDIR)/build/srpm
protocols := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
.PHONY: all check check-repositories check-sensor-proxy check-copr-trigger clean srpm validate
all: build/uke-control build/uke-control-packages build/uke-control-tests build/ssc-discovery build/ssc-sample
build:
	mkdir -p build
build/libuke-control.so.0: src/backend.cpp src/backend.hpp src/charge-policy.cpp src/charge-policy.hpp src/charge-client.cpp src/charge-client.hpp src/usb.cpp src/usb.hpp src/sysfs.hpp src/firmware.cpp src/firmware.hpp src/usb-request.cpp src/usb-request.hpp src/front-color.cpp src/front-color.hpp src/rpm-api.hpp src/desktop.cpp src/desktop.hpp src/hardware.cpp src/hardware.hpp src/wake.cpp src/wake.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -shared -Wl,-soname,libuke-control.so.0 -o $@ src/backend.cpp src/charge-policy.cpp src/charge-client.cpp src/usb.cpp src/firmware.cpp src/usb-request.cpp src/front-color.cpp src/desktop.cpp src/hardware.cpp src/wake.cpp $(shell pkg-config --libs libselinux libsystemd)
build/uke-control: src/main.cpp src/sensors.cpp src/sensors.hpp src/charge-notify.cpp src/charge-notify.hpp build/libuke-control.so.0
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ src/main.cpp src/sensors.cpp src/charge-notify.cpp -Lbuild -l:libuke-control.so.0 $(shell pkg-config --libs libsystemd)
build/uke-control-packages: src/packages.cpp | build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(shell pkg-config --cflags --libs libdnf5 rpm)
build/xdg-shell-client.h: | build
	wayland-scanner client-header $(protocols)/stable/xdg-shell/xdg-shell.xml $@
build/presentation-time-client.h: | build
	wayland-scanner client-header $(protocols)/stable/presentation-time/presentation-time.xml $@
build/%-protocol.c: | build
	case '$*' in xdg-shell) xml=$(protocols)/stable/xdg-shell/xdg-shell.xml;; presentation-time) xml=$(protocols)/stable/presentation-time/presentation-time.xml;; esac
	wayland-scanner private-code "$$xml" $@
build/%-protocol.o: build/%-protocol.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@ $(shell pkg-config --cflags wayland-client)
build/uke-control-tests: src/diagnostics.cpp src/display-device.hpp src/cadence.cpp src/contact.cpp src/desktop.cpp src/desktop.hpp src/hardware.cpp src/hardware.hpp build/xdg-shell-client.h build/presentation-time-client.h build/xdg-shell-protocol.o build/presentation-time-protocol.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ src/diagnostics.cpp src/cadence.cpp src/contact.cpp src/desktop.cpp src/hardware.cpp build/*-protocol.o $(LDLIBS)
build/ssc-discovery: shared/ssc-discovery.cpp | build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<
build/ssc-sample: shared/ssc-sample.cpp shared/ssc-discovery.cpp | build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<
build/test-front-color: tests/front-color.cpp src/front-color.cpp src/front-color.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/front-color.cpp $(shell pkg-config --libs libsystemd) -o $@
check: all build/test-front-color
	$(CXX) $(CXXFLAGS) $(LDFLAGS) tests/wake.cpp src/wake.cpp -o build/test-wake
	build/test-wake
	bash tests/wake-policy.sh
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/display-device.cpp -o build/test-display-device
	build/test-display-device
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/rpm-api.cpp -o build/test-rpm-api
	build/test-rpm-api
	build/test-front-color
	LD_LIBRARY_PATH=build build/uke-control --self-test
	build/uke-control-tests --self-test
	bash tests/charge-policy.sh
	bash tests/swap-discovery.sh
	bash tests/swap-allocation.sh
	bash tests/report.sh
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/charge-client.cpp src/charge-client.cpp $(shell pkg-config --libs libsystemd) -o build/test-charge-client
	build/test-charge-client
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/charge-notify.cpp src/charge-notify.cpp $(shell pkg-config --libs libsystemd) -o build/test-charge-notify
	build/test-charge-notify
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -pthread tests/charge-notify-bus.cpp $(shell pkg-config --libs libsystemd) -o build/test-charge-notify-bus
	dbus-run-session -- build/test-charge-notify-bus
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -pthread tests/desktop-bus.cpp src/desktop.cpp $(shell pkg-config --libs libsystemd) -o build/test-desktop-bus
	dbus-run-session -- build/test-desktop-bus
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/usb.cpp -Lbuild -l:libuke-control.so.0 -o build/test-usb
	LD_LIBRARY_PATH=build build/test-usb
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/firmware.cpp -Lbuild -l:libuke-control.so.0 -o build/test-firmware
	LD_LIBRARY_PATH=build build/test-firmware
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/hardware.cpp src/hardware.cpp -o build/test-hardware
	build/test-hardware
	for name in discovery sample; do $(CXX) $(CXXFLAGS) $(LDFLAGS) tests/ssc-$$name.cpp -o build/test-ssc-$$name; build/test-ssc-$$name; done
	LD_LIBRARY_PATH=build build/uke-control --help
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) tests/sensors.cpp src/sensors.cpp src/front-color.cpp $(shell pkg-config --libs libsystemd) -o build/test-sensors
	build/test-sensors
	$(CXX) $(CXXFLAGS) $(LDFLAGS) tests/packages.cpp $(shell pkg-config --cflags --libs libdnf5 rpm) -o build/test-packages
	build/test-packages
check-repositories: build/test-package-repos
	bash tests/package-repos.sh
build/test-package-repos: tests/package-repos.cpp src/packages.cpp | build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< $(shell pkg-config --cflags --libs libdnf5 rpm) -o $@
check-sensor-proxy: build/test-sensor-proxy
	bash tests/sensor-proxy.sh
check-copr-trigger:
	bash tests/copr-trigger.sh
build/test-sensor-proxy: tests/sensor-proxy.cpp src/sensors.cpp src/front-color.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $< src/front-color.cpp $(shell pkg-config --libs libsystemd) -o $@
validate:
	test -s src/backend.cpp -a -s src/diagnostics.cpp -a -s packaging/uke-control.spec
	cmp shared/ssc-discovery.cpp ../uke-sensors/src/ssc-discovery.cpp
	cmp shared/ssc-sample.cpp ../uke-sensors/src/ssc-sample.cpp
srpm: validate
	top="$$(realpath -m '$(outdir)/rpmbuild')"
	mkdir -p "$$top"/{SOURCES,SPECS,SRPMS}
	tar --sort=name --mtime=@1792270800 --owner=0 --group=0 --numeric-owner --exclude=build --transform='s#^#uke-control-1.2.0/#' -czf "$$top/SOURCES/uke-control-1.2.0.tar.gz" src shared tests packaging host Makefile LICENSE README.md CHARGING.md DISPLAY.md CHANNELS.md SENSORS.md DESKTOP.md HARDWARE.md
	cp packaging/uke-control.spec "$$top/SPECS/"
	rpmbuild -bs --nodeps --target aarch64 --define "_topdir $$top" "$$top/SPECS/uke-control.spec"
	install -m644 "$$top/SRPMS/"*.src.rpm '$(outdir)/'
clean:
	rm -rf build
