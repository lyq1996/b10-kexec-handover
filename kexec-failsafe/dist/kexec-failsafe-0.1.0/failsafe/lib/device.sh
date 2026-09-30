#!/bin/sh

DEV_MODEL=""; DEV_BOARD=""; DEV_KERNEL=""; DEV_HOST=""; DEV_CPU=""; DEV_NCPU=""

_cpu_part_name(){
    case "$1" in
        0xd03) echo Cortex-A53 ;;
        0xd05) echo Cortex-A55 ;;
        0xd07) echo Cortex-A57 ;;
        0xd09) echo Cortex-A73 ;;
        0xd0b) echo Cortex-A76 ;;
        0xd0d) echo Cortex-A77 ;;
        0xd41) echo Cortex-A78 ;;
        0xd44) echo Cortex-X1 ;;
        0xd46) echo Cortex-A510 ;;
        0xd47) echo Cortex-A710 ;;
        0xd48) echo Cortex-X2 ;;
        0xd4d) echo Cortex-A715 ;;
        0xd4e) echo Cortex-X3 ;;
        *)     echo "" ;;
    esac
}

_cpu_pretty(){
    printf '%s' "$1" | awk -F- '{
        out = toupper(substr($1,1,1)) substr($1,2)
        for (i = 2; i <= NF; i++) out = out "-" toupper($i)
        printf "%s", out
    }'
}

dev_read(){
    DEV_MODEL=""; DEV_BOARD=""; DEV_KERNEL=""; DEV_HOST=""; DEV_CPU=""; DEV_NCPU=""

    if has ubus; then
        _b="$(ubus call system board 2>/dev/null | tr -d '\n\t')"
        if [ -n "$_b" ]; then
            DEV_MODEL="$(printf '%s' "$_b"  | sed -n 's/.*"model":[[:space:]]*"\([^"]*\)".*/\1/p')"
            DEV_BOARD="$(printf '%s' "$_b"  | sed -n 's/.*"board_name":[[:space:]]*"\([^"]*\)".*/\1/p')"
            DEV_KERNEL="$(printf '%s' "$_b" | sed -n 's/.*"kernel":[[:space:]]*"\([^"]*\)".*/\1/p')"
            DEV_HOST="$(printf '%s' "$_b"   | sed -n 's/.*"hostname":[[:space:]]*"\([^"]*\)".*/\1/p')"
        fi
    fi

    [ -n "$DEV_MODEL" ] || DEV_MODEL="$(tr -d '\0' < /proc/device-tree/model 2>/dev/null)"
    [ -n "$DEV_KERNEL" ] || DEV_KERNEL="$(uname -r 2>/dev/null)"

    _pmu="$(tr -d '\0' < /proc/device-tree/pmu/compatible 2>/dev/null)"
    case "$_pmu" in
        *cortex-*)
            DEV_CPU="$(_cpu_pretty "$(printf '%s' "$_pmu" | sed -n 's/.*\(cortex-[a-z0-9]*\).*/\1/p')")"
            ;;
    esac

    if [ -z "$DEV_CPU" ]; then
        _part="$(sed -n 's/^CPU part[[:space:]]*:[[:space:]]*//p' /proc/cpuinfo 2>/dev/null | head -1)"
        if [ -n "$_part" ]; then
            _name="$(_cpu_part_name "$_part")"
            DEV_CPU="${_name:-$_part}"
        fi
    fi

    DEV_NCPU="$(grep -c '^processor' /proc/cpuinfo 2>/dev/null)"
    [ -n "$DEV_NCPU" ] && [ "$DEV_CPU" != "" ] && DEV_CPU="$DEV_CPU ×$DEV_NCPU"
}

dev_json(){
    printf '{"model":"%s","board":"%s","kernel":"%s","hostname":"%s","cpu":"%s","ncpu":"%s"}' \
        "$(json_str "$DEV_MODEL")" "$(json_str "$DEV_BOARD")" \
        "$(json_str "$DEV_KERNEL")" "$(json_str "$DEV_HOST")" \
        "$(json_str "$DEV_CPU")" "$(json_str "$DEV_NCPU")"
}
