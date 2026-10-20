# Contributor and agent instructions

- Keep changes within their owning component. Read component-specific packaging, source and license rules first.
- Use English for public documentation. Keep docs casual and human-readable. State the device, test environment and verified outcome; distinguish build success from real hardware support.
- Preserve imported source provenance in `SOURCE-IMPORTS.json` and component records. Do not edit donor snapshots or execute them as shortcuts.
- Write new native device applications in C++ where appropriate; preserve upstream language conventions elsewhere. Prefer Bash for host automation.
- Keep the original Fedora KDE applications. Do not clone or rebrand them as Uke forks.
- Treat firmware, factory calibration, charging controls, boot integration and USB power behavior as safety-sensitive. Never claim hardware functionality from software tests alone.
- Do not expose private logs, credentials, device identifiers, calibration values or personal paths.
- Run `make validate` and relevant native tests for affected components. Keep device test records clearly separate and report untested behavior.
- Preserve meaningful investigation records in the component's existing documentation when required, without putting long test logs into README.md.
