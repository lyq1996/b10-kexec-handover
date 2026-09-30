#!/bin/sh

FS_DIR="$(cd "$(dirname "$0")" && pwd)"
for _l in log env config redline device led wps payload p34 jump console; do
    . "$FS_DIR/lib/$_l.sh"
done

MODE=start
FOREGROUND=0
for _a in "$@"; do
    case "$_a" in
        -f|--foreground) FOREGROUND=1 ;;
        --check|--probe) MODE="$_a" ;;
        *) die "unknown argument: $_a (use -f, --check, --probe)" ;;
    esac
done

config_ensure && config_apply || die "config read failed"

check(){
    _rc=0
    say "== read-only self-check =="

    for _f in "$KEXEC_DIR/kjump" "$KEXEC_DIR/$KO_NAME" \
              "$THTTPD_BIN" "$HTTP_DOCROOT/index.html" "$FS_DIR/start.sh"; do
        if [ -f "$_f" ]; then
            say "$(printf '   %-46s %9s B' "$_f" "$(fsize "$_f")")"
        else
            warn "   $(printf '%-46s' "$_f") MISSING"
            _rc=1
        fi
    done

    say "   free space       $(df -h "$DF_PATH" 2>/dev/null | tail -1 | awk '{print $4}')  ($DF_PATH)"

    if [ -b "$OPT_DEV" ]; then
        say "   opt partition    $OPT_DEV  $(blockdev --getsize64 "$OPT_DEV" 2>/dev/null) B"
    else
        warn "   opt partition    not found $OPT_DEV"
        _rc=1
    fi

    redline_read
    say "   redline          $(redline_summary)"

    if deps_check; then
        say "   external cmds    ok"
    else
        _rc=1
    fi

    return $_rc
}

case "$MODE" in
    --check)
        check || die "self-check failed"
        say "self-check passed"
        exit 0
        ;;
    --probe)
        say "== control plane bootability check =="
        if console_probe; then
            say "   pass: thttpd starts and responds normally"
            exit 0
        fi
        die "failed: control plane will not start -- must not jump now"
        ;;
esac

check || die "self-check failed, aborted"

say "== starting control plane (thttpd) =="
say "   docroot    $HTTP_DOCROOT"
say "   CGI match  $HTTP_CGIPAT"
say "   port       $HTTP_PORT"
say "   listen     0.0.0.0 (all interfaces)"
say "   URL        http://<host LAN address>:$HTTP_PORT"
say "   this UI has no authentication, do not expose to untrusted networks."

if [ "$FOREGROUND" = 1 ]; then
    say "   mode       foreground"
    say "   stop       Ctrl+C"
    console_foreground
    _fg_rc=$?
    say "control plane stopped (rc=$_fg_rc)"
    exit "$_fg_rc"
fi

console_ensure || die "control plane failed to start or self-test failed"

led_hold 2>/dev/null || true

say "   stop       kill \$(cat $THTTPD_PID)"
