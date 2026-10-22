SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
.PHONY: validate test-native srpm
version := 1.0.0
package := $(notdir $(CURDIR))
outdir ?= $(CURDIR)/build/srpm
validate:
	jq -e '.schema_version == 1 and .device == "uke" and .soc == "SM7675" and .hardware_tested == false' manifests/component.json >/dev/null
	jq -e '.device == "uke" and .hardware_tested == false and .boot_tested == false' src/profile.json >/dev/null
	if grep -Eq '^(Requires|Recommends):.*(nabu)' packaging/*.spec; then exit 1; fi
	bash tests/plasma-profile.sh
test-native: validate
srpm: validate
	top="$$(realpath -m "$(outdir)/rpmbuild")"
	mkdir -p "$$top"/{BUILD,BUILDROOT,RPMS,SRPMS,SOURCES,SPECS}
	tar --sort=name --mtime=@1791158400 --owner=0 --group=0 --numeric-owner -cJf "$$top/SOURCES/$(package)-$(version).tar.xz" --transform 's,^,$(package)-$(version)/,' LICENSE README.md src tests packaging
	cp packaging/$(package).spec "$$top/SPECS/"
	rpmbuild -bs --nodeps --target aarch64 --define '_buildhost uke-builder' --define "_topdir $$top" "$$top/SPECS/$(package).spec"
	mkdir -p "$(outdir)"
	install -m644 "$$top/SRPMS/"*.src.rpm "$(outdir)/"
