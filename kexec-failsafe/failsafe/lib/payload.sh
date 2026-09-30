#!/bin/sh

PL_IMG=""; PL_DTB=""; PL_IMG_SIZE=0; PL_DTB_SIZE=0; PL_OK=0; PL_WHY=""

_img_magic_ok(){
    _m="$(dd if="$1" bs=1 skip=56 count=4 2>/dev/null | od -An -tx1 | tr -d ' \n')"
    [ "$_m" = "41524d64" ]
}

payload_read(){
    PL_IMG="$(cfg kernel)"
    PL_DTB="$(cfg dtb)"
    PL_IMG_SIZE=0; PL_DTB_SIZE=0; PL_OK=0; PL_WHY=""

    if [ -z "$PL_IMG" ] || [ ! -f "$PL_IMG" ]; then
        PL_WHY="kernel-missing"; return 1
    fi
    if [ -z "$PL_DTB" ] || [ ! -f "$PL_DTB" ]; then
        PL_WHY="dtb-missing"; return 1
    fi

    PL_IMG_SIZE="$(fsize "$PL_IMG")"
    PL_DTB_SIZE="$(fsize "$PL_DTB")"

    if [ "$PL_DTB_SIZE" -gt $(( DTB_MAX_KB * 1024 )) ]; then
        PL_WHY="dtb-too-big"; return 1
    fi

    if ! _img_magic_ok "$PL_IMG"; then
        PL_WHY="image-magic"; return 1
    fi

    PL_OK=1
    return 0
}

payload_json(){
    printf '{"image":%s,"image_size":%s,"dtb":%s,"dtb_size":%s,"ok":%s,"why":"%s"}' \
        "$([ "$PL_IMG_SIZE" -gt 0 ] && echo true || echo false)" "$PL_IMG_SIZE" \
        "$([ "$PL_DTB_SIZE" -gt 0 ] && echo true || echo false)" "$PL_DTB_SIZE" \
        "$([ "$PL_OK" = 1 ] && echo true || echo false)" \
        "$(json_str "$PL_WHY")"
}
