#!/bin/sh
. "$(cd "$(dirname "$0")/../.." && pwd)/lib/cgi.sh"

hdr_json

_i_act="$(form_get "$QUERY_STRING" action)"
[ -n "$_i_act" ] || _i_act=put

mkdir -p "$UPLOAD_DIR" 2>/dev/null

_safe_name(){
    case "$1" in
        ''|.|..|*/*|-*) return 1 ;;
    esac
    printf '%s' "$1"
}

case "$_i_act" in

put)
    _i_what="$(form_get "$QUERY_STRING" what)"
    case "$_i_what" in
        image) _i_max=$(( IMG_MAX_MB * 1024 * 1024 )) ;;
        dtb)   _i_max=$(( DTB_MAX_KB * 1024 )) ;;
        *)     printf '{"ok":false,"error":"bad-what","text":"unknown file type"}'; exit 0 ;;
    esac

    _i_name="$(_safe_name "$(form_get "$QUERY_STRING" name)")" \
        || { printf '{"ok":false,"error":"bad-name","text":"invalid filename"}'; exit 0; }

    _i_path="$UPLOAD_DIR/$_i_name"

    _i_cl="$(_content_length)"
    if [ "$_i_cl" -le 0 ]; then
        printf '{"ok":false,"error":"empty","text":"no data received"}'
        exit 0
    fi

    if [ "$_i_cl" -gt "$_i_max" ]; then
        body_read "$_i_max" > "$_i_path"
        body_drain "$_i_max"
        rm -f "$_i_path" 2>/dev/null
        printf '{"ok":false,"error":"too-big","size":%s,"max":%s,"text":"file exceeds limit"}' \
            "$_i_cl" "$_i_max"
        exit 0
    fi

    body_read "$_i_max" > "$_i_path"

    _i_sz="$(fsize "$_i_path")"
    if [ "$_i_sz" -ne "$_i_cl" ]; then
        warn "temp boot: $_i_what short write (got $_i_sz / declared $_i_cl bytes)"
        rm -f "$_i_path" 2>/dev/null
        printf '{"ok":false,"error":"truncated","got":%s,"want":%s,"text":"transfer truncated"}' \
            "$_i_sz" "$_i_cl"
        exit 0
    fi

    say "temp boot: $_i_what written $_i_name ($_i_sz bytes)"
    printf '{"ok":true,"what":"%s","name":"%s","size":%s,"max":%s}' \
        "$_i_what" "$(json_str "$_i_name")" "$_i_sz" "$_i_max"
    ;;

jump)
    _i_img="$(_safe_name "$(form_get "$QUERY_STRING" image)")" \
        || { printf '{"ok":false,"error":"bad-name","text":"invalid image filename"}'; exit 0; }
    _i_dtb="$(_safe_name "$(form_get "$QUERY_STRING" dtb)")" \
        || { printf '{"ok":false,"error":"bad-name","text":"invalid device tree filename"}'; exit 0; }

    _i_img="$UPLOAD_DIR/$_i_img"
    _i_dtb="$UPLOAD_DIR/$_i_dtb"

    [ -f "$_i_img" ] || { printf '{"ok":false,"error":"image-missing","text":"kernel image not uploaded yet"}'; exit 0; }
    [ -f "$_i_dtb" ] || { printf '{"ok":false,"error":"dtb-missing","text":"device tree not uploaded yet"}'; exit 0; }

    _i_cmd="$(form_get "$QUERY_STRING" cmdline)"
    [ -n "$_i_cmd" ] || _i_cmd="$IRFS_CMDLINE"

    "$KEXEC_DIR/kjump" --info --kernel "$_i_img" --dtb "$_i_dtb" >/dev/null 2>&1
    _i_rc=$?
    if [ "$_i_rc" -ne 0 ]; then
        warn "temp boot precheck failed rc=$_i_rc ($(jump_rc_text "$_i_rc"))"
        printf '{"ok":false,"error":"precheck","rc":%s,"text":"%s"}' \
            "$_i_rc" "$(jump_rc_text "$_i_rc")"
        exit 0
    fi

    say "temp boot: precheck passed, jumping immediately (image=$_i_img dtb=$_i_dtb)"
    jump_clear_result
    printf '{"ok":true,"msg":"jumping","image_size":%s,"dtb_size":%s}' \
        "$(fsize "$_i_img")" "$(fsize "$_i_dtb")"
    _sleep_ms 300

    (
        _i_t0="$(date +%s)"
        jump_exec "$_i_img" "$_i_dtb" "$_i_cmd"
        _i_rc=$?
        warn "temp boot jump failed rc=$_i_rc ($(jump_rc_text "$_i_rc")) after $(( $(date +%s) - _i_t0 ))s"
        jump_record_result "$_i_rc" irfs
    ) >/dev/null 2>&1 </dev/null &
    ;;

*)
    printf '{"ok":false,"error":"bad-action"}'
    ;;
esac
