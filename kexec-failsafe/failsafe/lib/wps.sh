#!/bin/sh

wps_pressed(){
    if has devmem; then
        _v="$(devmem "$WPS_REG" 32 2>/dev/null)"
        if [ -n "$_v" ]; then
            _n=$(( _v ))
            [ $(( _n & 1 )) -eq 0 ] && return 0
            return 1
        fi
    fi

    if [ -r /sys/kernel/debug/gpio ]; then
        grep -qE 'gpio-37[[:space:]]+\(.*\)[[:space:]]+in[[:space:]]+lo' \
            /sys/kernel/debug/gpio 2>/dev/null && return 0
    fi

    return 1
}

wps_available(){
    has devmem && return 0
    [ -r /sys/kernel/debug/gpio ] && return 0
    return 1
}

wps_wait(){
    _sec="$1"
    _ms="$WPS_POLL_MS"
    _rounds=$(( _sec * 1000 / _ms ))
    [ "$_rounds" -lt 1 ] && _rounds=1

    _i=0
    while [ "$_i" -lt "$_rounds" ]; do
        wps_pressed && return 0
        _sleep_ms "$_ms"
        _i=$(( _i + 1 ))
    done
    return 1
}
