# POSIX shell fragment embedded only in RPM migration scriptlets.
# Restore the admitted state; never enable a previously disabled hardware path.
case "$phase" in
save)
    [ -d /run/systemd/system ] || exit 0
    test "$(rpm -q --qf '%{NAME}' "$old" 2>/dev/null)" = "$old" || exit 0
    state=/run/uke-rpm-migration
    [ ! -L "$state" ] && [ ! -L "$state/$new" ] || exit 1
    install -d -m700 "$state"
    enabled=$(systemctl is-enabled "$unit" 2>/dev/null) || :
    active=$(systemctl is-active "$unit" 2>/dev/null) || :
    case "$enabled" in enabled|enabled-runtime|disabled|static|indirect|masked|masked-runtime) ;; *) exit 1;; esac
    case "$active" in active|inactive|failed|activating|deactivating) ;; *) exit 1;; esac
    (umask 077; printf '%s\n%s\n' "$enabled" "$active" >"$state/$new")
    ;;
restore)
    if [ "$new" = uke-iio-sensor-proxy ]; then
        if /usr/bin/selinuxenabled; then
            /usr/bin/semodule -i /usr/share/selinux/packages/uke-ssc.cil || exit 1
        else
            /usr/bin/semodule -n -s targeted -i /usr/share/selinux/packages/uke-ssc.cil || exit 1
        fi
    fi
    state=/run/uke-rpm-migration
    [ -f "$state/$new" ] && [ ! -L "$state" ] && [ ! -L "$state/$new" ] || exit 0
    enabled=$(sed -n '1p' "$state/$new")
    active=$(sed -n '2p' "$state/$new")
    current=$(systemctl is-enabled "$unit" 2>/dev/null) || :
    case "$current" in masked|masked-runtime) unlink "$state/$new"; exit 0;; esac
    systemctl daemon-reload || exit 1
    case "$enabled" in
        enabled) systemctl enable "$unit" || exit 1;;
        enabled-runtime) systemctl enable --runtime "$unit" || exit 1;;
        masked|masked-runtime) unlink "$state/$new"; exit 0;;
    esac
    if [ "$active" = active ]; then systemctl start "$unit" || exit 1; fi
    unlink "$state/$new"
    ;;
*) exit 1;;
esac
