SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
.PHONY: srpm
# COPR reads this root rule, with cwd set to the configured source subdirectory.
srpm:
	# Git is absent from COPR's initial source chroot. This rule's location is
	# already supplied by COPR, so root discovery needs no additional tool.
	root="$(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)"
	subdir="$$(realpath --relative-to="$$root" .)"
	if [[ "$$subdir" == packaging/families/uke-* ]]; then
		family="$${subdir##*/}"
		case "$$family" in uke-core|uke-hw|uke-power|uke-sensors|uke-extras|uke-additional|uke-metas|uke-camera|uke-sound) ;; *) exit 1;; esac
		dnf -y --setopt=install_weak_deps=False install make curl jq xz tar gzip rpm-build coreutils cpio gawk bash kf6-rpm-macros systemd-rpm-macros cmake-rpm-macros meson gettext python3-rpm-macros cargo-rpm-macros
		stage="$$root/packaging/families/build/$$family"
		$(MAKE) -C "$$root/packaging/families" srpm FAMILY="$$family" outdir="$$stage"
		mkdir -p "$(outdir)"
		install -m644 "$$stage/SRPMS/"*.src.rpm "$(outdir)/"
		exit 0
	fi
	case "$$subdir" in
		uke-core-meta|uke-desktop-metas|uke-orangefox-packaging|uke-boot|uke-desktop-integration|uke-control) component="$$subdir"; package=;;
		uke-sensors/packaging/libssc|uke-sensors/packaging/iio-sensor-proxy|uke-sensors/packaging/uke-ssc-fileservice)
			component=uke-sensors; package="$${subdir##*/}";;
		uke-desktop-packaging/packaging/*)
			component=uke-desktop-packaging; package="$${subdir##*/}"
			case "$$package" in material-decoration|plymouth-uke|at-spi2-core|gstreamer1|libaccounts-glib|libwacom|libstdcxx-uke-runtime|mesa) ;; *) echo 'Unreviewed or withdrawn source package' >&2; exit 1;; esac;;
		*) echo 'Unreviewed component or source directory' >&2; exit 1;;
	esac
	dnf -y --setopt=install_weak_deps=False install make curl jq xz tar gzip rpm-build coreutils cpio gawk bash kf6-rpm-macros systemd-rpm-macros
	stage="$$root/$$component/build/copr-source"
	if test -n "$$package"; then
		$(MAKE) -C "$$root/$$component" srpm PACKAGE="$$package" outdir="$$stage/$$package"
		stage="$$stage/$$package"
	else
		$(MAKE) -C "$$root/$$component" srpm outdir="$$stage"
	fi
	mkdir -p "$(outdir)"
	install -m644 "$$stage/"*.src.rpm "$(outdir)/"
