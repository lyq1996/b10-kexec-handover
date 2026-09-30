#!/bin/sh

CFG_KEYS="kernel dtb cmdline irfs_cmdline mem_base mem_size bootlog ko_name \
log_path log_max log_keep log_floor log_rotate \
opt_dev opt_mnt upload_dir df_path \
kexec_dir http_docroot thttpd_bin thttpd_pid thttpd_log \
http_port http_cgipat http_user http_bind \
chunk_size exec_timeout \
wps_reg wps_window wps_poll_ms led_wps qfprom_auth \
bootlog_max_kb dtb_max_kb img_max_mb \
ext4_label ext4_features ext4_opts"

_json_get(){
    sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" "$2" 2>/dev/null | head -1
}

config_ensure(){
    [ -f "$DEFAULTS_JSON" ] || { warn "missing $DEFAULTS_JSON"; return 1; }
    mkdir -p "$STATE_DIR" 2>/dev/null

    if [ ! -f "$CONFIG_CONF" ]; then
        : > "$CONFIG_CONF"
        for _k in $CFG_KEYS; do
            _v="$(_json_get "$_k" "$DEFAULTS_JSON")"
            printf '%s=%s\n' "$_k" "$_v" >> "$CONFIG_CONF"
        done
        [ -f "$MODE_FILE" ] || echo opt > "$MODE_FILE"
        return 0
    fi

    _want=$(printf '%s\n' $CFG_KEYS | wc -l)
    _have=$(grep -c '^[A-Za-z_][A-Za-z0-9_]*=' "$CONFIG_CONF" 2>/dev/null)
    [ "$_have" = "$_want" ] && {
        [ -f "$MODE_FILE" ] || echo opt > "$MODE_FILE"
        return 0
    }

    for _k in $CFG_KEYS; do
        grep -q "^$_k=" "$CONFIG_CONF" 2>/dev/null && continue
        _v="$(_json_get "$_k" "$DEFAULTS_JSON")"
        printf '%s=%s\n' "$_k" "$_v" >> "$CONFIG_CONF"
        warn "config key added (new in this version): $_k=$_v"
    done
    [ -f "$MODE_FILE" ] || echo opt > "$MODE_FILE"
    return 0
}

config_apply(){
    config_ensure || return 1

    KERNEL="$(cf kernel)"
    DTB="$(cf dtb)"
    CMDLINE="$(cf cmdline)"
    IRFS_CMDLINE="$(cf irfs_cmdline)"
    MEM_BASE="$(cf mem_base)"
    MEM_SIZE="$(cf mem_size)"
    BOOTLOG="$(cf bootlog)"
    KO_NAME="$(cf ko_name)"

    OPT_DEV="$(cf opt_dev)"
    OPT_MNT="$(cf opt_mnt)"
    OPT_BOOT_DIR="$OPT_MNT/boot"
    UPLOAD_DIR="$(cf upload_dir)"
    DF_PATH="$(cf df_path)"
    [ -n "$DF_PATH" ] || DF_PATH="$(dirname "$(cf log_path)")"

    KEXEC_DIR="$(cf kexec_dir)"
    [ -n "$KEXEC_DIR" ] || KEXEC_DIR="$(dirname "$FS_DIR")/kexec"
    HTTP_DOCROOT="$(cf http_docroot)"
    [ -n "$HTTP_DOCROOT" ] || HTTP_DOCROOT="$WWW_DIR"
    THTTPD_BIN="$(cf thttpd_bin)"
    [ -n "$THTTPD_BIN" ] || THTTPD_BIN="$FS_DIR/bin/thttpd"
    THTTPD_PID="$(cf thttpd_pid)"
    THTTPD_LOG="$(cf thttpd_log)"

    HTTP_PORT="$(cf http_port)"
    HTTP_CGIPAT="$(cf http_cgipat)"
    HTTP_USER="$(cf http_user)"
    HTTP_BIND="$(cf http_bind)"

    CHUNK_SIZE="$(cf chunk_size)"
    EXEC_TIMEOUT="$(cf exec_timeout)"
    WPS_REG="$(cf wps_reg)"
    WPS_WINDOW_SEC="$(cf wps_window)"
    WPS_POLL_MS="$(cf wps_poll_ms)"
    LED_WPS_G="$(cf led_wps)"
    QFPR0M_AUTH="$(cf qfprom_auth)"
    BOOTLOG_HARD_MAX_KB="$(cf bootlog_max_kb)"
    DTB_MAX_KB="$(cf dtb_max_kb)"
    IMG_MAX_MB="$(cf img_max_mb)"
    EXT4_LABEL="$(cf ext4_label)"
    EXT4_FEATURES="$(cf ext4_features)"
    EXT4_OPTS="$(cf ext4_opts)"

    LOG_PATH_CFG="$(cf log_path)"
    LOG_MAX_CFG="$(cf log_max)"
    LOG_KEEP_CFG="$(cf log_keep)"
    LOG_FLOOR_CFG="$(cf log_floor)"
    LOG_ROTATE_CFG="$(cf log_rotate)"

    for _k in CHUNK_SIZE HTTP_PORT WPS_POLL_MS WPS_WINDOW_SEC BOOTLOG_HARD_MAX_KB \
              DTB_MAX_KB IMG_MAX_MB EXEC_TIMEOUT; do
        eval "_v=\$$_k"
        [ -n "$_v" ] || { warn "missing config entry: $_k (check $CONFIG_CONF)"; return 1; }
    done

    return 0
}

config_set(){
    config_ensure || return 1
    _tmp="$CONFIG_CONF.tmp"
    grep -v "^$1=" "$CONFIG_CONF" > "$_tmp" 2>/dev/null
    printf '%s=%s\n' "$1" "$2" >> "$_tmp"
    mv -f "$_tmp" "$CONFIG_CONF"
}

config_reset(){
    rm -f "$CONFIG_CONF" 2>/dev/null
    config_ensure
}

mode_get(){ cat "$MODE_FILE" 2>/dev/null || echo opt; }
mode_set(){
    case "$1" in
        opt|hold|stay) echo "$1" > "$MODE_FILE" ;;
        *) return 1 ;;
    esac
}

json_str(){ printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }

config_json(){
    printf '{'
    printf '"kernel":"%s",'   "$(json_str "$KERNEL")"
    printf '"dtb":"%s",'      "$(json_str "$DTB")"
    printf '"cmdline":"%s",'  "$(json_str "$CMDLINE")"
    printf '"irfs_cmdline":"%s",' "$(json_str "$IRFS_CMDLINE")"
    printf '"mem_base":"%s",' "$(json_str "$MEM_BASE")"
    printf '"mem_size":"%s",' "$(json_str "$MEM_SIZE")"
    printf '"bootlog":"%s",'  "$(json_str "$BOOTLOG")"
    printf '"ko":"%s",'       "$(json_str "$KO_NAME")"
    printf '"root":"%s"'      "$(json_str "$OPT_DEV")"
    printf '}'
}
