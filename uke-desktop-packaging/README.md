# Uke desktop packaging

This component holds optional desktop packages and historical Rawhide source
recipes. The Xiaomi Pad 7 Fedora 44 image builder currently uses original
Fedora KDE and GNOME packages; it does not build these native runtime variants.

Package sources are in `packaging/`, local downloaded sources are kept in
ignored `source-cache/`, and build output is under `build/`. Use `make validate`
to check the component metadata. A package build is not a hardware test.
