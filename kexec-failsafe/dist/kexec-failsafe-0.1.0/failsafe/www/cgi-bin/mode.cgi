#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_json
_body="$(form_body)"
_m="$(form_get "$_body" mode)"

if [ -n "$_m" ]; then
    if mode_set "$_m"; then
        say "boot mode -> $_m"
        printf '{"ok":true,"mode":"%s"}' "$_m"
    else
        printf '{"ok":false,"error":"bad-mode"}'
    fi
else
    printf '{"ok":true,"mode":"%s"}' "$(mode_get)"
fi
