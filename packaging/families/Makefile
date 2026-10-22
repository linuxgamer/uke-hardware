SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
FAMILY ?= uke-core
outdir ?= $(CURDIR)/build/$(FAMILY)
.PHONY: srpm check
srpm:
	bash host/compose.sh '$(FAMILY)' '$(outdir)'
check:
	bash tests/split-spec.sh
	bash tests/cohort.sh
	bash tests/service-rename.sh
	bash tests/contracts.sh
	bash tests/rawhide.sh
