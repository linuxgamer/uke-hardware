SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
CXX ?= c++
CXXFLAGS ?= -O2 -g
prefix ?= /usr
outdir ?= $(CURDIR)/build
battery_sources := src/battery/main.cpp src/battery/collector.cpp src/battery/service.cpp src/battery/session.cpp src/battery/checkpoint.cpp src/battery/profile.cpp src/battery/record.cpp
control_sources := src/battery/control-main.cpp src/battery/control.cpp src/battery/control-profile.cpp src/battery/record.cpp
.PHONY: all validate test-native install srpm
all: $(outdir)/uke-battery $(outdir)/uke-charge-control $(outdir)/uke-offline-charge
$(outdir)/uke-offline-charge: src/offline/main.cpp src/offline/boot-mode.hpp src/offline/io.hpp src/offline/screen.hpp src/offline/sampler.hpp src/battery/provider.hpp
	mkdir -p "$(outdir)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -ffile-prefix-map=$(CURDIR)=. src/offline/main.cpp $$(pkg-config --cflags --libs libdrm) $(LDFLAGS) -o "$@"
$(outdir)/uke-battery: $(battery_sources) src/battery/collector.hpp src/battery/model.hpp src/battery/session.hpp src/battery/profile.hpp src/battery/record.hpp src/battery/provider.hpp src/battery/demand.hpp
	mkdir -p "$(outdir)"
	pkg-config --atleast-version=257 libsystemd
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -pthread -ffile-prefix-map=$(CURDIR)=. $(battery_sources) $$(pkg-config --cflags --libs libsystemd) $(LDFLAGS) -o "$@"
$(outdir)/uke-charge-control: $(control_sources) src/battery/control.hpp src/battery/record.hpp src/battery/provider.hpp
	mkdir -p "$(outdir)"
	pkg-config --atleast-version=257 libsystemd
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror -pthread -ffile-prefix-map=$(CURDIR)=. $(control_sources) $$(pkg-config --cflags --libs libsystemd libcrypto) $(LDFLAGS) -o "$@"
validate:
	jq -e '.schema_version == 1 and .device == "uke" and .soc == "SM7675" and .hardware_tested == false and .python_target_payload == false and (.expected_packages | length > 0)' manifests/component.json >/dev/null
	jq -e '.kind == "qualification_record" and .enable_control == false and .battery_pack_id == null and .allowed_vbus_uv == [] and .jeita_bands == [] and .qualification == "unqualified"' manifests/battery-profile.json >/dev/null
	jq -e '.kind == "model_qualification" and .qualification == "unqualified" and .enable_control == false and .identities == null and .evidence_id == null' manifests/model-profile.json >/dev/null
	jq -e '.kind == "control_qualification" and .qualification == "unqualified" and .enable_control == false and .current_control == false and .current_limits == null and .standard_clients == false' manifests/control-profile.json >/dev/null
	jq -e 'all(.features[]; .qualified == false) and .tests_executed == false' manifests/battery-readiness.json >/dev/null
	jq -e '.qualification == "unqualified" and .physical_acceptance == false and .pack_limits == null and all(.features[]; .qualified == false)' manifests/protection-readiness.json >/dev/null
test-native:
	mkdir -p "$(outdir)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror tests/battery-demand.cpp -o "$(outdir)/test-battery-demand"
	"$(outdir)/test-battery-demand"
	bash tests/battery-model.sh
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror tests/battery-engineering.cpp -o "$(outdir)/test-battery-engineering"
	"$(outdir)/test-battery-engineering"
	bash tests/battery-session.sh
	bash tests/battery-profile.sh
	bash tests/battery-collector.sh
	bash tests/battery-api.sh
	bash tests/charge-control.sh
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror tests/offline-charge.cpp -o "$(outdir)/test-offline-charge"
	"$(outdir)/test-offline-charge"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -Wall -Wextra -Werror tests/offline-sampler.cpp -o "$(outdir)/test-offline-sampler"
	"$(outdir)/test-offline-sampler"
install: all
	install -Dm755 "$(outdir)/uke-battery" "$(DESTDIR)$(prefix)/bin/uke-battery"
	install -Dm755 "$(outdir)/uke-charge-control" "$(DESTDIR)$(prefix)/libexec/uke-charge-control"
	install -Dm755 "$(outdir)/uke-offline-charge" "$(DESTDIR)$(prefix)/libexec/uke-offline-charge"
	install -Dm644 packaging/systemd/uke-offline-charge.service "$(DESTDIR)$(prefix)/lib/systemd/system/uke-offline-charge.service"
	install -Dm644 packaging/systemd/uke-offline-charge.target "$(DESTDIR)$(prefix)/lib/systemd/system/uke-offline-charge.target"
	install -Dm644 manifests/control-profile.json "$(DESTDIR)/etc/uke-battery/control-profile.json"
	install -Dm644 manifests/protection-readiness.json "$(DESTDIR)$(prefix)/share/uke-battery/protection-readiness.json"
	install -Dm644 packaging/upower/60-uke-battery-safety.conf "$(DESTDIR)/etc/UPower/UPower.conf.d/60-uke-battery-safety.conf"
	install -Dm644 packaging/systemd/uke-charge-control.service "$(DESTDIR)$(prefix)/lib/systemd/system/uke-charge-control.service"
	install -Dm644 packaging/systemd/uke-charge-automatic.service "$(DESTDIR)$(prefix)/lib/systemd/system/uke-charge-automatic.service"
	install -Dm644 packaging/systemd/uke-charge-protection.service "$(DESTDIR)$(prefix)/lib/systemd/system/uke-charge-protection.service"
	install -Dm644 packaging/udev/80-uke-charge-protection.rules "$(DESTDIR)$(prefix)/lib/udev/rules.d/80-uke-charge-protection.rules"
	install -Dm644 packaging/systemd/91-uke-charge-control.preset "$(DESTDIR)$(prefix)/lib/systemd/system-preset/91-uke-charge-control.preset"
	install -Dm644 packaging/systemd/uke-battery.service "$(DESTDIR)$(prefix)/lib/systemd/system/uke-battery.service"
	install -Dm644 packaging/systemd/90-uke-battery.preset "$(DESTDIR)$(prefix)/lib/systemd/system-preset/90-uke-battery.preset"
	install -Dm644 packaging/dbus/org.senemos.Uke.Battery1.conf "$(DESTDIR)$(prefix)/share/dbus-1/system.d/org.senemos.Uke.Battery1.conf"
	install -Dm644 packaging/dbus/org.senemos.Uke.Battery1.service "$(DESTDIR)$(prefix)/share/dbus-1/system-services/org.senemos.Uke.Battery1.service"
	install -Dm644 packaging/sysusers/uke-battery.conf "$(DESTDIR)$(prefix)/lib/sysusers.d/uke-battery.conf"
	install -Dm644 manifests/battery-profile.json "$(DESTDIR)$(prefix)/share/uke-battery/battery-profile.json"
	install -Dm644 manifests/model-profile.json "$(DESTDIR)$(prefix)/share/uke-battery/model-profile.json"
srpm:
	mkdir -p "$(outdir)/rpmbuild"/{BUILD,BUILDROOT,RPMS,SRPMS,SOURCES,SPECS}
	# Package exactly the reviewed worktree, with a per-file byte inventory.
	stage=$$(mktemp -d)
	trap 'rm -r -- "$$stage"' EXIT
	mkdir "$$stage/uke-battery-observer-1.1.0"
	cp -a Makefile LICENSE README.md src manifests docs packaging patches tests "$$stage/uke-battery-observer-1.1.0/"
	(cd "$$stage/uke-battery-observer-1.1.0"; find . -type f ! -name SOURCE-SHA256SUMS -print0 | LC_ALL=C sort -z | xargs -0 sha256sum > SOURCE-SHA256SUMS)
	tar --sort=name --mtime='2026-10-18 00:00:00Z' --owner=0 --group=0 --numeric-owner -cJf "$(outdir)/rpmbuild/SOURCES/uke-battery-observer-1.1.0.tar.xz" -C "$$stage" uke-battery-observer-1.1.0
	cp packaging/uke-battery-observer.spec "$(outdir)/rpmbuild/SPECS/"
	cp packaging/sysusers/uke-battery.conf "$(outdir)/rpmbuild/SOURCES/"
	rpmbuild -bs --nodeps --target aarch64 --define "_topdir $(outdir)/rpmbuild" "$(outdir)/rpmbuild/SPECS/uke-battery-observer.spec"
