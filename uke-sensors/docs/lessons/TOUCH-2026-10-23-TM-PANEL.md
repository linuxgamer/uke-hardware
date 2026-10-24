# TOUCH-2026-10-23-TM-PANEL

- Date: 23 October 2026.
- Environment: Xiaomi Pad 7 running OS3.0.304.0 TWRP; Fedora 44 KDE boot journal from the same tablet; host C++ checks.
- Evidence: the stock touch driver reports `lcd_id_value = 0, this panel is TM` and requests `novatek_nt36532_o82_fw_tm.bin`. TWRP reads GPIO 26 as input low. The Fedora image installed CSOT firmware and its service repeatedly stopped at `Touch panel-ID must be high`. The pinned TM normal image and configuration pass the host protocol and geometry checks.
- Practical consequence: the Fedora 44 image recipe defaults to TM. The daemon accepts pinned CSOT and TM images but requires the GPIO level to match the image before driving the controller. An explicit CSOT build option remains available.
- Correction: the TM image booted and reached Plasma setup on 24 October, but touch still failed because the daemon required a native panel reset provider. See [TOUCH-2026-10-24-FRAMEBUFFER-HANDOFF](TOUCH-2026-10-24-FRAMEBUFFER-HANDOFF.md).
- Uncertainty: touch input and stylus behavior remain unverified on the tablet.
- Next validation: install the framebuffer handoff fix, boot Fedora, and test touch and stylus input.
