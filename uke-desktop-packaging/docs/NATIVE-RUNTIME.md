# Historical native runtime recipes

These recipes are retained for the older Rawhide package workflow. The
Xiaomi Pad 7 Fedora 44 image builder uses Fedora's normal runtime packages,
including Python where required. It does not rebuild them to remove Python.

`manifests/native-runtime.json` records the reviewed source RPM identities.
`src/native-runtime/prepare-source.sh` prepares local sources under `build/`;
downloaded archives belong in ignored `source-cache/fedora-srpms/`. Run
`make validate` before packaging. These host checks do not qualify a tablet
boot or a graphical session.
