# Checking desktop RPMs

The Fedora 44 image builder selects original Fedora desktop packages. For a
new package recipe, verify its source and signatures, resolve dependencies
with DNF, then install the resulting RPMs in a clean AArch64 test root. Check
that the expected files and versions are present. Keep the exact package list
and checksums with the build output.

Python is allowed as a normal Fedora dependency. Do not remove RPM-owned files
from a composed root to change its dependency graph. A package test does not
establish a successful tablet boot or working display.
