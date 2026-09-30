#!/bin/sh

LOG_SRC="$(basename "$0" 2>/dev/null || echo sh)"

LOG_IS_CGI(){ [ -n "${KFS_CGI:-}${GATEWAY_INTERFACE:-}${REQUEST_METHOD:-}" ]; }
LOG_PATH(){   printf '%s' "$LOG_PATH_CFG"; }

_to_bytes(){
    _tb_v="$1"
    case "$_tb_v" in
        *[kK]) echo $(( ${_tb_v%[kK]} * 1024 )) ;;
        *[mM]) echo $(( ${_tb_v%[mM]} * 1024 * 1024 )) ;;
        *[gG]) echo $(( ${_tb_v%[gG]} * 1024 * 1024 * 1024 )) ;;
        *[0-9]) echo "$_tb_v" ;;
        *) echo 0 ;;
    esac
}

LOG_MAX(){    _to_bytes "${LOG_MAX_CFG:-4M}"; }
LOG_KEEP(){   printf '%s' "${LOG_KEEP_CFG:-3}"; }
LOG_FLOOR(){  _to_bytes "${LOG_FLOOR_CFG:-8M}"; }
LOG_ROTATE(){ printf '%s' "${LOG_ROTATE_CFG:-1}"; }

_log_free_kb(){
    df -k "$(dirname "$1")" 2>/dev/null | awk 'NR==2{print $4}'
}

_log_rotate(){
    _lr_f="$1"; _lr_keep="$2"; _lr_i="$_lr_keep"
    while [ "$_lr_i" -gt 1 ]; do
        [ -f "$_lr_f.$((_lr_i-1))" ] && mv -f "$_lr_f.$((_lr_i-1))" "$_lr_f.$_lr_i" 2>/dev/null
        _lr_i=$((_lr_i-1))
    done
    [ -f "$_lr_f" ] && mv -f "$_lr_f" "$_lr_f.1" 2>/dev/null
    : > "$_lr_f" 2>/dev/null
}

_log_append(){
    _la_f="$LOG_PATH_CFG"
    [ -n "$_la_f" ] || _la_f="$STATE_DIR/failsafe.log"
    [ -n "$_la_f" ] || return 0
    _la_d="$(dirname "$_la_f")"
    if ! mkdir -p "$_la_d" 2>/dev/null; then
        mkdir -p "$STATE_DIR" 2>/dev/null || return 0
        _la_f="$STATE_DIR/failsafe.log"
    fi

    _la_free="$(_log_free_kb "$_la_f")"
    _la_floor=$(( $(LOG_FLOOR) / 1024 ))
    if [ -n "$_la_free" ] && [ "$_la_floor" -gt 0 ] && [ "$_la_free" -lt "$_la_floor" ]; then
        return 0
    fi

    _la_cur="$(fsize "$_la_f")"
    _la_max="$(LOG_MAX)"
    if [ "$_la_max" -gt 0 ] && [ "$_la_cur" -ge "$_la_max" ]; then
        if [ "$(LOG_ROTATE)" = "1" ]; then
            _log_rotate "$_la_f" "$(LOG_KEEP)"
        else
            return 0
        fi
    fi

    printf '%s\n' "$1" >> "$_la_f" 2>/dev/null
    return 0
}

log(){
    _lg_lvl="${1:-INFO}"
    case "$_lg_lvl" in
        DEBUG|INFO|WARN|ERROR|FATAL) shift ;;
        *) _lg_lvl=INFO ;;
    esac
    _lg_line="$(date '+%F %T') [$LOG_SRC] [$_lg_lvl] $*"

    if LOG_IS_CGI; then
        _log_append "$_lg_line"
        return 0
    fi

    [ -n "$LOG_PATH_CFG" ] && _log_append "$_lg_line"

    case "$_lg_lvl" in
        WARN|ERROR|FATAL)
            printf '%s\n' "$_lg_line" >&2
            return 0
            ;;
    esac

    printf '%s\n' "$_lg_line"
}

say(){  log INFO  "$@"; }
warn(){ log WARN  "$@"; }
die(){  log FATAL "$@"; exit 1; }

log_to_term(){ [ -z "$LOG_PATH_CFG" ] && echo true || echo false; }

log_size(){
    [ -n "$LOG_PATH_CFG" ] || { echo 0; return 0; }
    fsize "$(LOG_PATH)"
}

log_free_kb(){
    if [ -z "$LOG_PATH_CFG" ]; then
        df -k "$STATE_DIR" 2>/dev/null | awk 'NR==2{print $4}'
    else
        _log_free_kb "$(LOG_PATH)"
    fi
}

log_clear(){
    [ -n "$LOG_PATH_CFG" ] || return 0
    _lc_f="$(LOG_PATH)"
    _lc_d="$(dirname "$_lc_f")"
    mkdir -p "$_lc_d" 2>/dev/null || {
        mkdir -p "$STATE_DIR" 2>/dev/null || return 0
        _lc_f="$STATE_DIR/failsafe.log"
    }
    rm -f "$_lc_f" "$_lc_f".* 2>/dev/null
    : > "$_lc_f" 2>/dev/null
}

log_tail(){
    [ -n "$LOG_PATH_CFG" ] || return 0
    _lt_f="$(LOG_PATH)"
    [ -f "$_lt_f" ] || return 0
    tail -n "${1:-200}" "$_lt_f" 2>/dev/null
}
