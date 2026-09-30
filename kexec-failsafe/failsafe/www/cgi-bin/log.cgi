#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

_action="$(form_get "$QUERY_STRING" action)"
_src="$(form_get "$QUERY_STRING" src)"
_lines="$(form_get "$QUERY_STRING" lines)"
[ -n "$_lines" ] || _lines=200

if [ "$_action" = "clear" ]; then
    form_body >/dev/null
    hdr_json
    log_clear
    say "operation log cleared"
    printf '{"ok":true}'
    exit 0
fi

hdr_text

case "$_src" in
    bootlog)
        _f="$(cfg bootlog)"
        [ -f "$_f" ] && tail -n "$_lines" "$_f" 2>/dev/null
        ;;

    dmesg)
        _o=""
        has dmesg && _o="$(dmesg 2>/dev/null)"
        if [ -n "$_o" ]; then
            printf '%s\n' "$_o" | tail -n "$_lines"
        else
            echo "(cannot read kernel dmesg)"
        fi
        ;;

    *)
        log_tail "$_lines"
        ;;
esac
