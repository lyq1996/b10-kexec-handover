#!/bin/sh

jump_prepare_bootlog(){
    _bl="$(cfg bootlog)"
    [ -n "$_bl" ] || return 0
    _dir="$(dirname "$_bl")"
    if ! mkdir -p "$_dir" 2>/dev/null; then
        mkdir -p "$STATE_DIR" 2>/dev/null || return 0
        _bl="$STATE_DIR/kjump-bootlog.txt"
    fi

    _sz="$(fsize "$_bl")"
    if [ "$_sz" -gt $(( BOOTLOG_HARD_MAX_KB * 1024 )) ]; then
        rm -f "$_bl" 2>/dev/null
    fi
    : > "$_bl" 2>/dev/null
    echo "$_bl"
}

jump_ko_modname(){
    _ko="$KEXEC_DIR/$KO_NAME"
    _n=""
    if has modinfo; then
        _n="$(modinfo -F name "$_ko" 2>/dev/null)"
    fi
    [ -n "$_n" ] || _n="$(printf '%s' "${KO_NAME%.ko}" | tr '-' '_')"
    printf '%s' "$_n"
}

jump_unload_ko(){
    _m="$(jump_ko_modname)"
    [ -n "$_m" ] || return 0
    if rmmod "$_m" 2>/dev/null; then
        say "unloading old module before jump: $_m"
    fi
    return 0
}

jump_reclaim(){
    has sync && sync 2>/dev/null
    if [ -w /proc/sys/vm/drop_caches ]; then
        echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
    fi
    return 0
}

jump_exec(){
    _je_kernel="$1"
    _je_dtb="$2"
    _je_cmd="$3"

    jump_unload_ko
    jump_reclaim
    _je_bl="$(jump_prepare_bootlog)"

    _je_out="$("$KEXEC_DIR/kjump" \
        --ko     "$KEXEC_DIR/$KO_NAME" \
        --kernel "$_je_kernel" \
        --dtb    "$_je_dtb" \
        --mem-base "$(cfg mem_base)" \
        --mem-size "$(cfg mem_size)" \
        --bootlog  "$_je_bl" \
        --append-cmdline "$_je_cmd" 2>&1)"
    _je_rc=$?

    if [ -n "$_je_out" ]; then
        printf '%s\n' "$_je_out" | while IFS= read -r _je_l; do
            say "$_je_l"
        done
        if [ -n "$_je_bl" ] && [ -f "$_je_bl" ]; then
            printf '%s\n' "$_je_out" >> "$_je_bl" 2>/dev/null
        fi
    fi
    return "$_je_rc"
}

jump_run(){
    say "jump: kernel=$(cfg kernel) dtb=$(cfg dtb)"
    jump_exec "$(cfg kernel)" "$(cfg dtb)" "$(cfg cmdline)"
    _rc=$?
    warn "jump failed: kjump rc=$_rc"
    return "$_rc"
}

jump_rc_text(){
    case "$1" in
        1)  echo "usage error" ;;
        2)  echo "ko read failed" ;;
        3)  echo "init_module failed" ;;
        4)  echo "dtb read / invalid FDT" ;;
        5)  echo "no /dev/kexec-lite" ;;
        6)  echo "LOAD failed (insufficient contiguous physical memory)" ;;
        7)  echo "JUMP returned unexpectedly (handover failed)" ;;
        8)  echo "secondary CPU offline failed" ;;
        9)  echo "kernel source parse failed" ;;
        10) echo "bootargs injection failed" ;;
        11) echo "ABI mismatch (module and kjump from different builds)" ;;
        *)  echo "unknown exit code" ;;
    esac
}

jump_result_file(){ printf '%s/lastjump' "$STATE_DIR"; }

jump_clear_result(){
    rm -f "$(jump_result_file)" 2>/dev/null
    return 0
}

jump_record_result(){
    _jr_rc="${1:-0}"
    _jr_src="${2:-unknown}"
    printf 'rc=%s\nsrc=%s\ntime=%s\ntext=%s\n' \
        "$_jr_rc" "$_jr_src" "$(date '+%F %T')" "$(jump_rc_text "$_jr_rc")" \
        > "$(jump_result_file)" 2>/dev/null
    return 0
}

jump_last_json(){
    _jl_f="$(jump_result_file)"
    if [ ! -f "$_jl_f" ]; then printf 'null'; return 0; fi
    _jl_rc="$(sed -n 's/^rc=//p' "$_jl_f" | head -1)"
    case "$_jl_rc" in ''|*[!0-9]*) _jl_rc=0 ;; esac
    printf '{"rc":%s,"src":"%s","time":"%s","text":"%s"}' \
        "$_jl_rc" \
        "$(json_str "$(sed -n 's/^src=//p' "$_jl_f" | head -1)")" \
        "$(json_str "$(sed -n 's/^time=//p' "$_jl_f" | head -1)")" \
        "$(json_str "$(sed -n 's/^text=//p' "$_jl_f" | head -1)")"
}
