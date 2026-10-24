SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
outdir ?= $(CURDIR)/build
version := 3.6.0
.PHONY: validate srpm

validate:
	jq -e '.schema_version == 1 and .buffybox.tag == "3.6.0" and .hardware_tested == false and (.buffybox.commit | test("^[a-f0-9]{40}$$")) and (.lvgl.commit | test("^[a-f0-9]{40}$$")) and (.source_sha256 | test("^[a-f0-9]{64}$$"))' source-lock.json >/dev/null
	bash -n scripts/stage-source.sh
	grep -Fq 'ConditionPathExists=/dev/fb0' packaging/uke-console-input.service
	grep -Fq 'ConditionPathExists=/dev/uinput' packaging/uke-console-input.service
	grep -Fq 'ConditionPathExists=/dev/input/uke-touchscreen' packaging/uke-console-input.service
	grep -Fq 'ATTRS{name}=="Novatek NT36532 Uke Touchscreen"' packaging/80-uke-console-input.rules
	grep -Fq 'Novatek NT36532 Uke Touchscreen' ../uke-sensors/src/touch-thp.cpp
	grep -Fq 'SYMLINK+="input/uke-touchscreen"' packaging/80-uke-console-input.rules
	grep -Fq 'ENV{SYSTEMD_WANTS}+="uke-console-input.service"' packaging/80-uke-console-input.rules
	! grep -Eq '^(Requires|BindsTo|Conflicts)=.*getty@tty1' packaging/uke-console-input.service

srpm: validate
	top="$(outdir)/rpmbuild"
	mkdir -p "$$top"/{BUILD,BUILDROOT,RPMS,SRPMS,SOURCES,SPECS}
	bash scripts/stage-source.sh "$$top/SOURCES/buffybox-$(version)-with-lvgl.tar.xz"
	install -m644 packaging/uke-console-input.spec "$$top/SPECS/"
	install -m644 packaging/uke-console-input.service packaging/80-uke-console-input.rules packaging/uke-console-input.modules-load.conf "$$top/SOURCES/"
	rpmbuild -bs --nodeps --target aarch64 --define "_topdir $$top" "$$top/SPECS/uke-console-input.spec"
