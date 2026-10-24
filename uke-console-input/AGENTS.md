# Contributor and agent instructions

- Keep this component limited to the experimental Fedora 44 console keyboard.
- Do not edit the imported BuffyBox or LVGL sources. Update pinned commits and provenance when changing upstream versions.
- Host builds cannot establish touch alignment or first-boot usability on Xiaomi Pad 7. Keep the service independent of getty.
- Never log typed keys or passwords.
