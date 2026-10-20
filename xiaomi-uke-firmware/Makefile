SHELL := /bin/bash
.PHONY: validate srpm verify-local-blobs verify-fedora44-firmware fedora44-image-data-srpm
validate:
	jq -e '.schema_version == 1 and .device == "uke" and .soc == "SM7675" and .hardware_tested == false and .python_target_payload == false and (.expected_packages | length > 0)' manifests/component.json >/dev/null
	jq -e '.schema_version==1 and .device=="uke" and (.admitted_files|length)==0 and .firmware_blob_package_ready==false' manifests/firmware-lock.json >/dev/null
	bash -n src/audit-admission.sh
	bash -n src/stage-local.sh
	jq -e '.schema_version==1 and .device=="uke" and .soc=="SM7675" and .public_upload_allowed==false and (.files|length)==74 and all(.files[]; .redistribution_review_complete==false)' manifests/local-firmware-candidates.json >/dev/null
	jq -e '.schema_version==1 and .device=="uke" and .public_upload_allowed==false and .per_device_data_included==false and (.entries|length)>0' manifests/local-blobs.json >/dev/null
	bash -n src/verify-local-blobs.sh
srpm:
	@echo 'No validated Uke package payload yet; complete the manifest acceptance gates.' >&2
	@exit 1
# A lock with no admitted bytes can validate structurally, but never build a
# firmware RPM. There is no metadata-only substitute for a firmware payload.
audit-admission:
	bash src/audit-admission.sh "$(LOCK)" "$(ARCHIVE)" "$(REQUESTS)"
.PHONY: audit-admission test-admission
test-admission: validate
	bash tests/admission.sh
	bash tests/stage-local.sh
.PHONY: stage-local
stage-local: validate
	bash src/stage-local.sh manifests/local-firmware-candidates.json "$(MODEM_FILES)" "$(GPU_FILES)" "$(OUTPUT)"
.PHONY: verify-local-blobs
verify-local-blobs:
	bash src/verify-local-blobs.sh
verify-fedora44-firmware:
	bash src/verify-fedora44-firmware.sh
fedora44-image-data-srpm: verify-local-blobs verify-fedora44-firmware
	@set -Eeuo pipefail; \
	root=payload/oem-os3.0.303.0; \
	test -d "$$root/bt/image" && test -f "$$root/bt/image/msbtfw12.mbn" && test -f "$$root/bt/image/msnv12.bin"; \
	top="$$(realpath -m build/fedora44-image-data/rpmbuild)"; \
	mkdir -p "$$top"/{SOURCES,SPECS,SRPMS}; \
	tar -C "$$root" --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
	    --transform='s#^#uke-fedora44-oem-data-3.0.303.0/#' \
	    -czf "$$top/SOURCES/uke-fedora44-oem-data-3.0.303.0.tar.gz" ssc bt firmware; \
	cp packaging/uke-fedora44-oem-data.spec "$$top/SPECS/"; \
	rpmbuild -bs --nodeps --target noarch --define "_topdir $$top" \
	    "$$top/SPECS/uke-fedora44-oem-data.spec"
