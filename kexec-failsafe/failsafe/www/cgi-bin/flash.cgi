#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_json

_action="$(form_get "$QUERY_STRING" action)"

case "$_action" in
    begin)
        flash_begin "$(form_get "$QUERY_STRING" size)" "$(form_get "$QUERY_STRING" sha)"
        ;;
    chunk)
        flash_chunk "$(form_get "$QUERY_STRING" id)" "$(form_get "$QUERY_STRING" off)"
        ;;
    finish)
        flash_finish "$(form_get "$QUERY_STRING" id)"
        ;;
    abort)
        flash_abort "$(form_get "$QUERY_STRING" id)"
        ;;
    *)
        printf '{"error":"bad-action"}'
        ;;
esac
