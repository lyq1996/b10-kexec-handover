#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_json
form_body >/dev/null

payload_read
if [ "$PL_OK" != 1 ]; then
    printf '{"ok":false,"error":"payload","why":"%s"}' "$PL_WHY"
    exit 0
fi

redline_read
say "manual jump (from control plane)"
jump_clear_result
printf '{"ok":true,"msg":"jumping"}'
_sleep_ms 300

(
    _t0="$(date +%s)"
    jump_run
    _rc=$?
    warn "manual jump failed rc=$_rc ($(jump_rc_text "$_rc")) after $(( $(date +%s) - _t0 ))s"
    jump_record_result "$_rc" manual
) >/dev/null 2>&1 </dev/null &
