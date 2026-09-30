#!/bin/sh

[ -n "${FS_DIR:-}" ] || die "FS_DIR not set (caller must compute the package directory first)"
export FS_DIR

LIB_DIR="$FS_DIR/lib"
WWW_DIR="$FS_DIR/www"
CGI_DIR="$WWW_DIR/cgi-bin"
STATE_DIR="$FS_DIR/state"
export LIB_DIR WWW_DIR CGI_DIR STATE_DIR

DEFAULTS_JSON="$STATE_DIR/defaults.json"
CONFIG_CONF="$STATE_DIR/config.conf"
MODE_FILE="$STATE_DIR/mode"
export DEFAULTS_JSON CONFIG_CONF MODE_FILE

KERNEL=""; DTB=""; CMDLINE=""; IRFS_CMDLINE=""; MEM_BASE=""; MEM_SIZE=""; BOOTLOG=""; KO_NAME=""
LOG_PATH_CFG=""; LOG_MAX_CFG=""; LOG_KEEP_CFG=""; LOG_FLOOR_CFG=""; LOG_ROTATE_CFG=""
OPT_DEV=""; OPT_MNT=""; OPT_BOOT_DIR=""; UPLOAD_DIR=""; DF_PATH=""
KEXEC_DIR=""; HTTP_DOCROOT=""; THTTPD_BIN=""; THTTPD_PID=""; THTTPD_LOG=""
HTTP_PORT=""; HTTP_CGIPAT=""; HTTP_USER=""; HTTP_BIND=""
CHUNK_SIZE=""; EXEC_TIMEOUT=""
WPS_REG=""; WPS_WINDOW_SEC=""; WPS_POLL_MS=""; LED_WPS_G=""; QFPR0M_AUTH=""
BOOTLOG_HARD_MAX_KB=""; DTB_MAX_KB=""; IMG_MAX_MB=""
EXT4_LABEL=""; EXT4_FEATURES=""; EXT4_OPTS=""
export KERNEL DTB CMDLINE IRFS_CMDLINE MEM_BASE MEM_SIZE BOOTLOG KO_NAME
export LOG_PATH_CFG LOG_MAX_CFG LOG_KEEP_CFG LOG_FLOOR_CFG LOG_ROTATE_CFG
export OPT_DEV OPT_MNT OPT_BOOT_DIR UPLOAD_DIR DF_PATH
export KEXEC_DIR HTTP_DOCROOT THTTPD_BIN THTTPD_PID THTTPD_LOG
export HTTP_PORT HTTP_CGIPAT HTTP_USER HTTP_BIND
export CHUNK_SIZE EXEC_TIMEOUT
export WPS_REG WPS_WINDOW_SEC WPS_POLL_MS LED_WPS_G QFPR0M_AUTH
export BOOTLOG_HARD_MAX_KB DTB_MAX_KB IMG_MAX_MB
export EXT4_LABEL EXT4_FEATURES EXT4_OPTS



PATH="/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export PATH

BUNDLE_BB="$FS_DIR/bin/busybox"
export BUNDLE_BB

_BB_LIST=""
_bb_has(){
    [ -x "$BUNDLE_BB" ] || return 1
    if [ -z "$_BB_LIST" ]; then
        _BB_LIST="$("$BUNDLE_BB" --list 2>/dev/null)"
        [ -n "$_BB_LIST" ] || { _BB_LIST="-"; return 1; }
    fi
    [ "$_BB_LIST" = "-" ] && return 1
    printf '%s\n' "$_BB_LIST" | grep -qx "$1"
}

od(){       _bb_has od       && "$BUNDLE_BB" od "$@"       || command od "$@"; }
blockdev(){ _bb_has blockdev && "$BUNDLE_BB" blockdev "$@" || command blockdev "$@"; }
timeout(){  _bb_has timeout  && "$BUNDLE_BB" timeout "$@"  || command timeout "$@"; }

deps_check(){
    _miss=""
    for _c in dd df awk sed tr head tail sleep grep mkdir rm mv wc date printf \
              cat cut sort xargs stat mktemp basename dirname kill ps \
              od blockdev timeout; do
        command -v "$_c" >/dev/null 2>&1 || _miss="$_miss $_c"
    done
    [ -n "$_miss" ] && { warn "missing external commands: $_miss"; return 1; }
    return 0
}

has(){ command -v "$1" >/dev/null 2>&1; }

fsize(){
    [ -f "$1" ] || { echo 0; return 0; }
    _n="$(wc -c < "$1" 2>/dev/null)"
    echo "${_n:-0}"
}

_sleep_ms(){
    _ms="$1"
    if has msleep; then
        msleep "$_ms"
    elif sleep 0.01 2>/dev/null; then
        sleep "$(awk "BEGIN{printf \"%.3f\", $_ms/1000}")"
    else
        _s=$(( _ms / 1000 )); [ "$_s" -lt 1 ] && _s=1
        sleep "$_s"
    fi
}

cfg(){
    [ -f "$CONFIG_CONF" ] || return 1
    sed -n "s/^$1=//p" "$CONFIG_CONF" | head -1
}

cf(){
    _v="$(cfg "$1")"
    [ -n "$_v" ] && { printf '%s' "$_v"; return 0; }
    printf '%s' "${2:-}"
}
