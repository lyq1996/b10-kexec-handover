#!/bin/sh

led_on(){  [ -w "$LED_WPS_G" ] && echo 1 > "$LED_WPS_G" 2>/dev/null; return 0; }
led_off(){ [ -w "$LED_WPS_G" ] && echo 0 > "$LED_WPS_G" 2>/dev/null; return 0; }

led_state(){ [ -r "$LED_WPS_G" ] && cat "$LED_WPS_G" 2>/dev/null; return 0; }

led_blink(){
    _n="${1:-3}"
    _i=0
    while [ "$_i" -lt "$_n" ]; do
        led_on;  _sleep_ms 150
        led_off; _sleep_ms 150
        _i=$((_i + 1))
    done
}

led_hold(){ led_on; }
