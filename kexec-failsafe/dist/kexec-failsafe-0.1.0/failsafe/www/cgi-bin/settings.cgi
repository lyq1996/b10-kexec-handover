#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_json
_body="$(form_body)"

if [ "$(form_get "$_body" action)" = "reset" ]; then
    config_reset
    say "settings restored to defaults"
    printf '{"ok":true,"reset":true}'
    exit 0
fi

_changed=0
_changed_keys=""
for _k in kernel dtb cmdline mem_base mem_size bootlog \
          log_path log_max log_keep log_floor log_rotate; do
    _v="$(form_get "$_body" "$_k")"
    if [ -n "$_v" ]; then
        config_set "$_k" "$_v"
        _changed=1
        _changed_keys="$_changed_keys $_k"
    fi
done

[ "$_changed" = 1 ] && say "settings saved, changed: $_changed_keys"
printf '{"ok":true,"changed":%s}' "$_changed"
