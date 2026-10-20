# Uke OS3.0.303.0 image inputs

Target: Xiaomi Pad 7 (`uke`, SM7675), Fedora 44. This directory contains the
touch, SSC and Bluetooth inputs extracted from the Xiaomi
`uke_global_images_OS3.0.303.0.WOZMIXM_16.0` package. `touch/reference.bin`
is a 5432-byte no-touch THP frame measured on one tablet. The source paths,
sizes and SHA-256 values are pinned in `../../manifests/local-blobs.json`.

Run `../../src/verify-local-blobs.sh` from any directory before building. The
Fedora 44 image builder performs the same check automatically and installs the
selected panel's touch inputs. SSC and Bluetooth files are staged for later
integration; this recipe does not currently install them.

No `persist` files, device addresses, serial numbers or security firmware are
included. The maintainer reports a redistribution grant for the OEM files; its
terms must be recorded and checked before the payload or images are pushed or
uploaded. Hardware behavior on other Xiaomi Pad 7 units remains unqualified.
