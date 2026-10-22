SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
.PHONY: validate srpm test-native test-plugin
outdir ?= $(CURDIR)/build/srpm
validate:
	jq -e '.schema_version == 1 and .device == "uke" and .soc == "SM7675" and .hardware_tested == false and .python_target_payload == false and (.expected_packages | index("uke-adaptive-display") != null)' manifests/component.json >/dev/null
	bash tests/adaptive-display.sh
srpm: validate
	top="$$(realpath -m '$(outdir)/rpmbuild')"
	mkdir -p "$$top"/{SOURCES,SPECS,SRPMS}
	tar --sort=name --mtime=@1792281600 --owner=0 --group=0 --numeric-owner --transform='s,^,uke-adaptive-display-1.0.16/,' -cf - LICENSE docs/ADAPTIVE-DISPLAY.md docs/DISPLAY-CONTROLS.md docs/SUSPEND.md docs/SERVICE-POLICY.md docs/GPU-MONITORING.md src/adreno-stats packaging/adreno.page packaging/uke-adaptive-display.target packaging/80-uke-adaptive-display.preset packaging/71-uke-tablet-mode.rules packaging/61-uke-display-ddc.rules packaging/60-block-scheduler.rules packaging/powerdevilrc packaging/kwinoutputconfig.json packaging/60-uke-display-ddc.conf packaging/30-uke-s2idle.conf packaging/uke-suspend.conf src/uke-ddc-config tests/ddc-config.sh | gzip -n > "$$top/SOURCES/uke-adaptive-display-1.0.16.tar.gz"
	cp packaging/uke-adaptive-display.spec "$$top/SPECS/"
	rpmbuild -bs --nodeps --target aarch64 --define "_topdir $$top" "$$top/SPECS/uke-adaptive-display.spec"
	install -m644 "$$top/SRPMS/"*.src.rpm '$(outdir)/'
test-native:
	mkdir -p build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Werror src/adreno-stats/tests.cpp -o build/adreno-counter-test
	build/adreno-counter-test

.PHONY: test-bluetooth
test-bluetooth:
	mkdir -p build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Werror -Wno-unused-function tests/bluetooth-address.cpp -o build/bluetooth-address-test
	build/bluetooth-address-test

test-plugin:
	cmake -S src/adreno-stats -B build/adreno-stats -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build build/adreno-stats --parallel 4
	ctest --test-dir build/adreno-stats --output-on-failure

# Host-only OEM data conversion; this executable is not a tablet startup task.
.PHONY: test-oem-brightness
test-oem-brightness:
	mkdir -p build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Werror tests/oem-brightness-curve.cpp -o build/oem-brightness-test
	./build/oem-brightness-test
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Werror src/oem-brightness-curve.cpp -o build/uke-oem-brightness-curve
