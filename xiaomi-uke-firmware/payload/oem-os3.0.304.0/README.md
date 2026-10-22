# Uke OS3.0.304.0 image inputs

This directory contains the model touch, SSC, Bluetooth, WPSS, ADSP, WLAN
board and GPU inputs for the Xiaomi Pad 7 Fedora 44 images. The files match
the sizes and SHA-256 values in `../../manifests/oem-runtime-source.json` and
`../../manifests/local-firmware-candidates.json`. Run
`../../src/verify-fedora44-oem-304.sh` to check the complete tree.

`touch/reference.bin` is a 5432-byte no-touch THP frame measured on one
tablet. No `persist` files, device addresses, serial numbers or calibration
data are bundled. Hardware behavior on other Xiaomi Pad 7 units remains
unqualified.
