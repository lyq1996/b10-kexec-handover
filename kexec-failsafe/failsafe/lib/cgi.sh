#!/bin/sh

exec 2>/dev/null

KFS_CGI=1
export KFS_CGI

CGI_DIR_SELF="$(cd "$(dirname "$0")" && pwd)"
[ -n "${FS_DIR:-}" ] || FS_DIR="$(cd "$CGI_DIR_SELF/../.." && pwd)"
export FS_DIR

for _l in log env config redline device led wps payload p34 jump console; do
    . "$FS_DIR/lib/$_l.sh"
done

hdr_json(){ printf 'Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n\r\n'; }
hdr_text(){ printf 'Content-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\n\r\n'; }

urldecode(){
    printf '%b' "$(printf '%s' "$1" | sed 's/+/ /g; s/%\([0-9A-Fa-f][0-9A-Fa-f]\)/\\x\1/g')"
}

form_get(){
    _v="$(printf '%s\n' "$1" | tr '&' '\n' | sed -n "s/^$2=//p" | head -1)"
    urldecode "$_v"
}

_content_length(){
    _cl="${CONTENT_LENGTH:-}"
    case "$_cl" in ''|*[!0-9]*) _cl=0 ;; esac
    printf '%s' "$_cl"
}

body_read(){
    _br_max="${1:-8192}"
    _br_cl="$(_content_length)"
    [ "$_br_cl" -gt 0 ] || return 1
    [ "$_br_cl" -le "$_br_max" ] || _br_cl="$_br_max"
    head -c "$_br_cl"
}

body_drain(){
    _bd_read="${1:-0}"
    _bd_cl="$(_content_length)"
    [ "$_bd_cl" -gt "$_bd_read" ] || return 0
    head -c "$(( _bd_cl - _bd_read ))" > /dev/null 2>&1
    return 0
}

form_body(){ body_read "${1:-8192}"; }

config_ensure && config_apply || {
    hdr_json
    printf '{"error":"config-load-failed"}'
    exit 1
}
