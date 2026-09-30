#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_text

_body="$(form_body)"
_cmd="$(form_get "$_body" cmd)"

if [ -z "$_cmd" ]; then
    echo "(empty command)"
    exit 0
fi

say "terminal: $_cmd"

if has timeout; then
    timeout "$EXEC_TIMEOUT" sh -c "$_cmd" 2>&1
else
    sh -c "$_cmd" 2>&1
fi
