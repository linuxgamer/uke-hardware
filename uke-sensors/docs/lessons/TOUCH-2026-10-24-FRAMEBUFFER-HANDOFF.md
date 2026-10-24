# TOUCH-2026-10-24-FRAMEBUFFER-HANDOFF

- Date: 24 October 2026.
- Environment: Xiaomi Pad 7, Fedora 44 KDE image built from OS3.0.304.0 inputs; Fedora boot journal read through TWRP; host C++ checks.
- Evidence: Fedora reached Plasma setup and the firmware framebuffer displayed it. `uke-touchscreen.service` repeatedly exited with `O82 kernel panel-reset provider is not ready`. The normal boot recipe marks its O82 panel handoff node disabled and provides `/chosen/framebuffer@...` as a simple framebuffer; the native panel reset provider is therefore absent in this mode.
- Practical consequence: the touch daemon now accepts the exact Uke firmware framebuffer handoff when no native panel provider exists. Native panel mode still requires its reset state. A firmware watchdog in framebuffer mode restarts the service for a bounded fresh initialization.
- Uncertainty: host tests cover admission and refusal, but touch operation and recovery on this tablet still require a boot test. The display's reported low frame rate is not measured by the journal.
- Next validation: install the new touchscreen RPM into the existing Fedora root, boot it, inspect the service journal, and test touch and the setup keyboard. Measure frame pacing separately before changing the native display path.
