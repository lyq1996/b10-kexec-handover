#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_json
config_ensure
dev_read
redline_read
payload_read

_opt_fs=""
_magic="$(ext4_magic)"
[ "$_magic" = "$EXT4_MAGIC" ] && _opt_fs="ext4"

_opt_size="$(blockdev --getsize64 "$OPT_DEV" 2>/dev/null || echo 0)"
_configs_free="$(df -k "$DF_PATH" 2>/dev/null | awk 'NR==2{print $4}')"
[ -n "$_configs_free" ] || _configs_free=0

printf '{'

printf '"device":%s,' "$(dev_json)"

printf '"redline":%s,' "$(redline_json)"

printf '"opt":{"dev":"%s","size":%s,"fstype":"%s","mountable":%s},' \
    "$OPT_DEV" "$_opt_size" "$_opt_fs" \
    "$([ -n "$_opt_fs" ] && echo true || echo false)"

printf '"boot_payload":%s,' "$(payload_json)"

printf '"boot_mode":"%s",' "$(mode_get)"

printf '"last_jump":%s,' "$(jump_last_json)"

printf '"recipe":%s,' "$(config_json)"

printf '"tool_dir":"%s","configs_free_kb":%s,' \
    "$(json_str "$KEXEC_DIR")" "$_configs_free"

printf '"paths":{"kexec_dir":"%s","ko":"%s","opt_dev":"%s","upload_dir":"%s","http_port":"%s","chunk":%s,"df_path":"%s"},' \
    "$(json_str "$KEXEC_DIR")" "$(json_str "$KO_NAME")" \
    "$(json_str "$OPT_DEV")" "$(json_str "$UPLOAD_DIR")" \
    "$(json_str "$HTTP_PORT")" "${CHUNK_SIZE:-0}" "$(json_str "$DF_PATH")"

printf '"log":{"path":"%s","size":%s,"max":"%s","keep":"%s","floor":"%s","free_kb":%s,"to_term":%s}' \
    "$(json_str "$(cfg log_path)")" "$(log_size)" \
    "$(json_str "$(cfg log_max)")" "$(json_str "$(cfg log_keep)")" \
    "$(json_str "$(cfg log_floor)")" "$(log_free_kb)" "$(log_to_term)"

printf '}'
