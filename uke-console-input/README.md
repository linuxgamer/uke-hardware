# Experimental console keyboard for Xiaomi Pad 7

This component packages upstream Buffyboard 3.6.0 for the Fedora 44 TTY image.
It draws on `/dev/fb0`, reads touch events through libinput and sends key events
through `/dev/uinput`. The systemd service starts only when those device nodes
exist. A missing keyboard does not block `getty@tty1.service` or first-boot
account setup; in that case an external keyboard or ADB recovery is required.

The source RPM contains the pinned BuffyBox and LVGL source trees, not a
postmarketOS binary. Run `make srpm` to fetch and check their exact commits.
Build the SRPM for Fedora 44 AArch64, then install `uke-console-input` only in
the TTY profile and enable `uke-console-input.service` there. The graphical
profiles remove the package and disable its service when they copy the TTY base.

This package has **not** been tested on Xiaomi Pad 7 hardware. Before calling
TTY touch-only setup usable, verify `/dev/fb0`, `/dev/uinput`, the Novatek touch
device, layout and rotation, password entry, login and keyboard recovery after
`uke-touchscreen.service` restarts. Buffyboard's keymap is US; the console
keymap must match. The service deliberately does not hide or replace tty1.
