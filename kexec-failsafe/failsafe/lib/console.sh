#!/bin/sh

console_selftest(){
    _port="$1"

    if has wget; then
        timeout 2 wget -q -O - "http://127.0.0.1:$_port/index.html" 2>/dev/null \
            | head -c 128 | grep -q -i "<!doctype\|<html" && return 0
        return 1
    fi

    if has netstat; then
        netstat -ltn 2>/dev/null | grep -q ":$_port" && return 0
    fi
    if [ -r /proc/net/tcp ]; then
        _hex="$(printf '%04X' "$_port")"
        awk 'NR>1{split($2,a,":"); if (a[2]=="'"$_hex"'") found=1} END{exit !found}' \
            /proc/net/tcp && return 0
    fi

    [ -f "$THTTPD_PID" ] && kill -0 "$(cat "$THTTPD_PID" 2>/dev/null)" 2>/dev/null
}

console_stop(){
    if [ -f "$THTTPD_PID" ]; then
        kill "$(cat "$THTTPD_PID" 2>/dev/null)" 2>/dev/null
        rm -f "$THTTPD_PID" 2>/dev/null
    fi
    return 0
}

console_precheck(){
    [ -x "$THTTPD_BIN" ]   || { warn "control plane: thttpd not executable ($THTTPD_BIN)"; return 1; }
    [ -r "$HTTP_DOCROOT/index.html" ] || { warn "control plane: index page not readable ($HTTP_DOCROOT/index.html)"; return 1; }
    [ -d "$HTTP_DOCROOT/cgi-bin" ]    || { warn "control plane: CGI directory missing"; return 1; }
    return 0
}

console_ensure(){
    console_precheck || return 1

    console_selftest "$HTTP_PORT" && return 0

    mkdir -p "$(dirname "$THTTPD_LOG")" 2>/dev/null
    mkdir -p "$(dirname "$THTTPD_PID")" 2>/dev/null

    "$THTTPD_BIN" -d "$HTTP_DOCROOT" -p "$HTTP_PORT" -c "$HTTP_CGIPAT" \
                  -u "$HTTP_USER" -l "$THTTPD_LOG" -i "$THTTPD_PID" 2>/dev/null &
    _bg=$!

    _i=0
    while [ "$_i" -lt 20 ]; do
        console_selftest "$HTTP_PORT" && return 0
        _sleep_ms 250
        _i=$(( _i + 1 ))
    done

    warn "control plane: self-test failed after start (port $HTTP_PORT in use?)"
    kill "$_bg" 2>/dev/null
    console_stop
    return 1
}

console_foreground(){
    console_precheck || return 1

    if console_selftest "$HTTP_PORT"; then
        warn "control plane: already serving on port $HTTP_PORT, stop it first"
        return 1
    fi

    mkdir -p "$(dirname "$THTTPD_LOG")" 2>/dev/null
    mkdir -p "$(dirname "$THTTPD_PID")" 2>/dev/null

    _cf_stop(){
        [ -n "${_cf_pid:-}" ] && kill "$_cf_pid" 2>/dev/null
        console_stop
        led_off 2>/dev/null
        return 0
    }

    trap '_cf_stop; exit 130' INT
    trap '_cf_stop; exit 143' TERM
    trap '_cf_stop; exit 129' HUP
    trap '_cf_stop' EXIT

    "$THTTPD_BIN" -D -d "$HTTP_DOCROOT" -p "$HTTP_PORT" -c "$HTTP_CGIPAT" \
                  -u "$HTTP_USER" -l "$THTTPD_LOG" -i "$THTTPD_PID" &
    _cf_pid=$!
    wait "$_cf_pid"
    _cf_rc=$?

    trap - INT TERM HUP EXIT
    return "$_cf_rc"
}

console_probe(){
    console_ensure || return 1
    console_stop
    return 0
}
