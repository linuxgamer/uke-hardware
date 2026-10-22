SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
.PHONY: validate srpm track-stable track-rawhide
PACKAGE ?= plymouth-uke
outdir ?= $(CURDIR)/build/srpm/$(PACKAGE)
validate:
	jq -e '.device == "uke" and .hardware_tested == false and .python_target_payload == false' manifests/component.json >/dev/null
	jq -e '(.commit|test("^[a-f0-9]{40}$$")) and (.sha256|test("^[a-f0-9]{64}$$")) and (.version|test("^[0-9]+[.][0-9]+$$")) and .hardware_specific==false and .hardware_tested==false' manifests/material-decoration.json >/dev/null
	test "$$(sed -n 's/^%global upstream_commit //p' packaging/material-decoration/material-decoration.spec)" = "$$(jq -r .commit manifests/material-decoration.json)"
	! grep -Eq '^(Requires|Recommends):.*(python|pypy|libpython|nabu)' packaging/*/*.spec
	jq -e '.hardware_tested==false and (.packages|length==9) and all(.packages[]; (.sha256|test("^[a-f0-9]{64}$$")) and (.native_release|test("^[0-9]+([.][0-9]+)*$$")) and (.version|test("uke";"i")|not) and .name!="plasma-workspace" and .name!="dolphin" and .name!="uke-power-profile-backend")' manifests/native-runtime.json >/dev/null
	test "$$(sha256sum src/native-runtime/fedora-exported-symbols.txt | cut -d ' ' -f1)" = "$$(jq -er .baseline.symbol_sha256 manifests/native-runtime.json)"
	test "$$(wc -l < src/native-runtime/fedora-exported-symbols.txt)" = "$$(jq -er .baseline.symbol_count manifests/native-runtime.json)"
	bash -n src/native-runtime/prepare-source.sh
srpm: validate
	case '$(PACKAGE)' in material-decoration|plymouth-uke|at-spi2-core|gstreamer1|libaccounts-glib|libwacom|libstdcxx-uke-runtime|mesa|upower|iio-sensor-proxy|accountsservice) ;; *) echo 'Unsupported or withdrawn package' >&2; exit 1;; esac
	top="$$(realpath -m "$(outdir)/rpmbuild")"
	mkdir -p "$$top"/{BUILD,BUILDROOT,RPMS,SRPMS,SOURCES,SPECS}
	if jq -e --arg name '$(PACKAGE)' 'any(.packages[]; .name==$$name)' manifests/native-runtime.json >/dev/null; then
		bash src/native-runtime/prepare-source.sh '$(PACKAGE)' "$$top" "$(CURDIR)"
		mkdir -p "$(outdir)"
		install -m644 "$$top/SRPMS/"*.src.rpm "$(outdir)/"
		exit 0
	fi
	if test '$(PACKAGE)' = material-decoration; then
		commit=$$(jq -er .commit manifests/material-decoration.json)
		archive="$(CURDIR)/referances/material-decoration/$$commit.tar.gz"
		mkdir -p "$$(dirname "$$archive")"
		if test ! -s "$$archive"; then
			curl -fL --retry 3 "https://codeload.github.com/guiodic/material-decoration/tar.gz/$$commit" -o "$$archive.part"
			mv "$$archive.part" "$$archive"
		fi
		test "$$(sha256sum "$$archive" | cut -d ' ' -f1)" = "$$(jq -er .sha256 manifests/material-decoration.json)"
		cp "$$archive" "$$top/SOURCES/material-decoration-$$commit.tar.gz"
		cp manifests/material-decoration.json "$$top/SOURCES/material-decoration-source-lock.json"
		cp packaging/material-decoration/*.patch "$$top/SOURCES/"
	else
		tar --sort=name --mtime=@1791158400 --owner=0 --group=0 --numeric-owner -cJf "$$top/SOURCES/uke-plymonth-1.0.2.tar.xz" --transform 's,^,uke-plymonth-1.0.2/,' LICENSE README.md src/plymouth-uke
	fi
	cp "packaging/$(PACKAGE)/$(PACKAGE).spec" "$$top/SPECS/"
	rpmbuild -bs --nodeps --target aarch64 --define "_topdir $$top" "$$top/SPECS/$(PACKAGE).spec"
	mkdir -p "$(outdir)"
	install -m644 "$$top/SRPMS/"*.src.rpm "$(outdir)/"
track-rawhide:
	bash ../packaging/families/host/track-rawhide.sh
track-stable:
	mkdir -p build/tracking
	curl -fsSL --retry 3 https://api.github.com/repos/guiodic/material-decoration/releases/latest -o build/tracking/release.json
	jq -e '.draft==false and .prerelease==false and (.tag_name|test("^[0-9]{2}[-.][0-9]{2}[-.][0-9]{2}$$"))' build/tracking/release.json >/dev/null
	tag=$$(jq -er .tag_name build/tracking/release.json)
	if test "$$tag" = "$$(jq -r .tag manifests/material-decoration.json)"; then echo 'Published stable pin unchanged'; exit 0; fi
	curl -fsSL --retry 3 "https://api.github.com/repos/guiodic/material-decoration/commits/$$tag" -o build/tracking/commit.json
	commit=$$(jq -er '.sha | select(test("^[a-f0-9]{40}$$"))' build/tracking/commit.json)
	version=$$(jq -er '.published_at' build/tracking/release.json | date -u -f - +%Y%m%d.%H%M%S)
	[[ "$$version" > "$$(jq -er .version manifests/material-decoration.json)" ]] || { echo 'Stable source version did not advance' >&2; exit 1; }
	archive="$(CURDIR)/referances/material-decoration/$$commit.tar.gz"
	mkdir -p "$$(dirname "$$archive")"
	curl -fL --retry 3 "https://codeload.github.com/guiodic/material-decoration/tar.gz/$$commit" -o "$$archive.part"
	tar -tzf "$$archive.part" > build/tracking/files.txt
	! grep -E '(^/|(^|/)[.][.](/|$$)|[.]py(c|o)?$$)' build/tracking/files.txt
	tar -xOzf "$$archive.part" "material-decoration-$$commit/LICENSE" | grep -F 'GNU GENERAL PUBLIC LICENSE' >/dev/null
	sha=$$(sha256sum "$$archive.part" | cut -d ' ' -f1)
	jq --arg tag "$$tag" --arg commit "$$commit" --arg version "$$version" --arg sha "$$sha" '.tag=$$tag | .commit=$$commit | .version=$$version | .sha256=$$sha' manifests/material-decoration.json > manifests/material-decoration.json.part
	mv "$$archive.part" "$$archive"
	mv manifests/material-decoration.json.part manifests/material-decoration.json
	sed -i "s/^%global upstream_commit .*/%global upstream_commit $$commit/; s/^Version:.*/Version:        $$version/" packaging/material-decoration/material-decoration.spec
	$(MAKE) validate
.PHONY: test-native
test-native:
	bash tests/native-migrations.sh
