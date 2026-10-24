SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
.PHONY: validate test-native
components := uke-core-meta uke-desktop-metas uke-desktop-packaging uke-boot uke-camera uke-desktop-integration uke-hardware-provenance uke-platform-runtime uke-sensors plasma-uke-kcm xiaomi-uke-firmware uke-control uke-console-input
validate:
	jq -e '.schema_version==1 and (.imports|length==11) and all(.imports[]; .imported_commit|test("^[a-f0-9]{40}$$")) and (.packaged_sources|length==2) and all(.packaged_sources[]; .component=="uke-console-input" and (.commit|test("^[a-f0-9]{40}$$")))' SOURCE-IMPORTS.json >/dev/null
	jq -e --slurpfile lock uke-console-input/source-lock.json 'all(.packaged_sources[]; . as $$source | any([$$lock[0].buffybox, $$lock[0].lvgl][]; .url==$$source.repository and .commit==$$source.commit))' SOURCE-IMPORTS.json >/dev/null
	for component in $(components); do $(MAKE) -C "$$component" validate; done
test-native:
	bash/tests/release-contract.sh
	$(MAKE) -C uke-sensors test-native
	$(MAKE) -C uke-desktop-packaging test-native
	$(MAKE) -C uke-boot test-native
	$(MAKE) -C uke-boot test-usb
	$(MAKE) -C uke-desktop-integration test-native
	$(MAKE) -C uke-platform-runtime test-native
	$(MAKE) -C uke-control check
	$(MAKE) -C xiaomi-uke-firmware test-admission
