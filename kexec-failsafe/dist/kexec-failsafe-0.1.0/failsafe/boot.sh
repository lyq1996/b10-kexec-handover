#!/bin/sh

FS_DIR="$(cd "$(dirname "$0")" && pwd)"
for _l in log env config redline device led wps payload p34 jump console; do
    . "$FS_DIR/lib/$_l.sh"
done

if ! { config_ensure && config_apply; }; then
    log FATAL "config read failed ($CONFIG_CONF) -- aborting jump, staying on stock"
    led_blink 6
    exit 1
fi
say "===== boot.sh starting ====="

redline_read
say "redline: $(redline_summary)"

payload_read
if [ "$PL_OK" != 1 ]; then
    warn "payload invalid ($PL_WHY) -> staying on stock"
    led_blink 6
    exit 0
fi
say "payload: Image ${PL_IMG_SIZE}B / dtb ${PL_DTB_SIZE}B"

_mode="$(mode_get)"
case "$_mode" in
    stay)
        say "mode stay -> staying on stock"
        exit 0
        ;;
    hold)
        say "mode hold -> staying on stock this time, clearing flag"
        mode_set opt
        exit 0
        ;;
esac

if wps_available; then
    say "WPS window open (${WPS_WINDOW_SEC}s)"
    led_blink 3
    if wps_wait "$WPS_WINDOW_SEC"; then
        say "WPS press detected"
        say "[temporary] auto enter control plane disabled, logging only"
        exit 0
    fi
    say "window closed without press"
else
    warn "WPS not detectable (no devmem / debugfs) -> skipping window"
fi

say "verifying control plane bootability before jump"
say "[temporary] control plane pre-start disabled -- to verify manually: sh start.sh --probe"
say "preparing jump: $KERNEL"
say "  command: $KEXEC_DIR/kjump --ko \"$KEXEC_DIR/$KO_NAME\" --kernel \"$KERNEL\" --dtb \"$DTB\" --mem-base \"$MEM_BASE\" --mem-size \"$MEM_SIZE\" --bootlog \"$BOOTLOG\" --append-cmdline \"$CMDLINE\""
say "[temporary] auto jump disabled -- to jump, run jump_run manually or use the control plane"
exit 0
