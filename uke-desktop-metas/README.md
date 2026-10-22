# Uke desktop selections

`uke-desktop-metas` records the console-first desktop policy. The optional
`uke-kde-plasma-meta` selects original Fedora Rawhide AArch64 Plasma applications.
It includes Discover's PackageKit backend, Dolphin plugins and administrative
access, KWrite, Chromium, display settings and the common desktop utilities.
Neither package contains a desktop application fork or a hardware service.

The Plasma selection includes original Fedora `plasma-login-manager` and
`kcm-plasmalogin`. It installs no login preset or activation scriptlet. On an
existing graphical installation, select the standard service explicitly:

```sh
sudo systemctl enable --force plasmalogin.service
```

Remove any previous display manager's explicit startup links before rebooting;
only one manager may run. Preserve the previous package for rollback during
validation. Sensor startup does not depend on a specific login manager.
The Fedora root may include normal distribution Python dependencies. The
previous console-only restriction and desktop exception are historical profile
metadata pending an upstream policy update; neither should cause Fedora
packages to be rebuilt solely to remove Python.

The application list was compared with the Nabu KDE manifest and image profile at
[`03a5bdf09ce206410e7f7f1f97871110e66ffa82`](https://github.com/MCC45TR/Nabu-Fedora-Rawhide-Builder/tree/03a5bdf09ce206410e7f7f1f97871110e66ffa82).
Nabu hardware packages and first-run setup are excluded. Uke display,
touch, graphics and sensors require their own drivers and physical tests.

Run `make validate`, `make test-native` and `make srpm` for source and policy
checks. COPR's root `.copr/Makefile` builds both RPMs from this source family.
The optional Plasma RPM owns `plasma.json`, which records its exact application
selection for image composition. Install it explicitly on a Fedora graphical
root; installing the console metadata alone does not select Plasma.

As of 18 October 2026, the revised profile is a package candidate. RPM build, dependency
resolution, graphical application behavior and tablet hardware acceptance are
separate checks. Detailed test records remain in the private project journal.
