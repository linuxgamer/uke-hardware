SHELL := /bin/bash
.PHONY: validate srpm
validate:
	jq -e '.schema_version == 1 and .device == "uke" and .soc == "SM7675" and .hardware_tested == false and (.expected_packages | length > 0)' manifests/component.json >/dev/null
srpm:
	@echo 'No validated Uke package payload yet; complete the manifest acceptance gates.' >&2
	@exit 1
