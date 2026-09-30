#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_json
form_body >/dev/null

redline_read
p34_wipe_format
