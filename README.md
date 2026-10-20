# Uke Linux Hardware Support

Device integration packages for **POCO Pad X1** and **Xiaomi Pad 7** (`uke`, Qualcomm SM7675), used with the Uke Fedora project.

This repository contains platform services, sensors, firmware packaging, boot integration and device settings. It complements the [kernel](https://github.com/linuxgamer/senemos-uke-kernel-mainline); it is not a complete operating-system image.

## Components

| Directory | What it does |
| --- | --- |
| `uke-boot` | Boot and USB debug integration |
| `uke-platform-runtime` | Shared platform runtime services |
| `uke-sensors` | Sensor detection and tools |
| `uke-control` | Device controls, power-related settings and diagnostics |
| `uke-camera` | Camera integration work |
| `uke-desktop-integration` | Integration with the Fedora desktop |
| `plasma-uke-kcm` | Work on a device settings panel |
| `uke-core-meta` | Packages for the base system |
| `uke-desktop-metas` | Desktop package selections |
| `uke-desktop-packaging` | Native desktop libraries, theme and decoration |
| `uke-orangefox-packaging` | Recovery image packaging |
| `xiaomi-uke-firmware` | Firmware source and licensing records |
| `uke-hardware-provenance` | Records of hardware source material |

## Development

```sh
make validate
make test-native
```

These commands check project rules and native tests; they do not verify physical hardware. Imported component origins are recorded in [SOURCE-IMPORTS.json](SOURCE-IMPORTS.json).

The desktop uses original Fedora KDE applications rather than mcc45tr's forks. Factory firmware and device-specific calibration remain separate inputs and must be handled carefully.

Related: [Uke Fedora](https://github.com/linuxgamer/uke-fedora) · [Kernel](https://github.com/linuxgamer/senemos-uke-kernel-mainline) · [Image builder](https://github.com/linuxgamer/uke-fedora-builder)

See [LICENSE](LICENSE) for project licensing; imported components may have their own licenses.
