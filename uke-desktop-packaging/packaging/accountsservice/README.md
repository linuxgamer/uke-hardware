# Optional TCB account database

21 October 2026. Fedora Rawhide `accountsservice-26.27.3-2.fc45` warns on every
account enumeration when the optional `/etc/tcb` directory is absent, even
after reading the ordinary shadow database. The patch ignores only `ENOENT`;
permission, I/O and malformed-path failures retain their diagnostics. It does
not create authentication directories or change account selection.

Source: [official Fedora source RPM](https://kojipkgs.fedoraproject.org/packages/accountsservice/26.27.3/2.fc45/src/accountsservice-26.27.3-2.fc45.src.rpm),
SHA-256 `394b5016ef1a895b2c8ecf348be692830b483fe6f4612a771c0d6b764f2171e6`.
The modified upstream file `src/daemon.c` retains its GPL-3.0-or-later license
and upstream attribution. Source family `uke-additional` Release 4 includes this
correction and follows the original Rawhide source through the common watcher.

Run `bash tests/accountsservice-tcb.sh SOURCE_DIRECTORY` from this component;
`--patched` tests the already prepared build tree without applying the patch again.
The host fixture compiles the patched directory-enumeration block with GLib;
it checks absent and existing directories, normal TCB entry enumeration and
retained malformed-path errors. An unprivileged run also checks permission
denial. It does not test the complete daemon, PAM,
account mutations or tablet authentication. Coherent COPR packaging and device
validation remain required before deployment.
