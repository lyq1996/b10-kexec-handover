#!/bin/sh


EXT4_MAGIC_OFF=1080
EXT4_MAGIC="53ef"


flash_begin(){
    _size="$1"; _sha="$2"

    [ -b "$OPT_DEV" ] || { printf '{"error":"no-device"}'; return 1; }

    case "$_size" in
        ''|*[!0-9]*) printf '{"error":"bad-size"}'; return 1 ;;
    esac
    [ "$_size" -gt 0 ] || { printf '{"error":"bad-size"}'; return 1; }

    _cap="$(blockdev --getsize64 "$OPT_DEV" 2>/dev/null || echo 0)"
    if [ "$_cap" -gt 0 ] && [ "$_size" -gt "$_cap" ]; then
        printf '{"error":"too-big","cap":%s}' "$_cap"; return 1
    fi

    mkdir -p "$UPLOAD_DIR" 2>/dev/null
    _id="flash-$$-$(date +%s 2>/dev/null || echo 0)"
    printf '%s\n' "$_size" > "$UPLOAD_DIR/$_id.size"
    printf '0\n'           > "$UPLOAD_DIR/$_id.done"
    [ -n "$_sha" ] && printf '%s\n' "$_sha" > "$UPLOAD_DIR/$_id.sha"

    say "flash start: $_size bytes, session $_id"
    printf '{"id":"%s","chunk":%s}' "$_id" "$CHUNK_SIZE"
}

flash_chunk(){
    _id="$1"; _off="$2"

    case "$_id" in ''|*[!A-Za-z0-9._-]*) printf '{"error":"bad-id"}'; return 1 ;; esac
    case "$_off" in ''|*[!0-9]*) printf '{"error":"bad-offset"}'; return 1 ;; esac
    [ -f "$UPLOAD_DIR/$_id.size" ] || { printf '{"error":"no-session"}'; return 1; }

    _blk=$(( _off / CHUNK_SIZE ))

    _fc_cl="$(_content_length)"
    [ "$_fc_cl" -gt 0 ] || { printf '{"error":"no-length"}'; return 1; }

    body_read "$CHUNK_SIZE" | dd bs=1M seek="$_blk" conv=notrunc of="$OPT_DEV" 2>/dev/null
    if [ $? -ne 0 ]; then
        printf '{"error":"write-failed"}'; return 1
    fi

    printf '%s\n' "$_off" > "$UPLOAD_DIR/$_id.done"
    printf '{"off":%s}' "$_off"
}

flash_finish(){
    _id="$1"
    _sf="$UPLOAD_DIR/$_id.size"
    [ -f "$_sf" ] || { printf '{"error":"no-session"}'; return 1; }

    _size="$(cat "$_sf" 2>/dev/null)"
    _last="$(cat "$UPLOAD_DIR/$_id.done" 2>/dev/null || echo 0)"
    _sha_want="$(cat "$UPLOAD_DIR/$_id.sha" 2>/dev/null)"

    _magic="$(dd if="$OPT_DEV" bs=1 skip=$EXT4_MAGIC_OFF count=2 2>/dev/null \
              | od -An -tx1 | tr -d ' \n')"
    if [ "$_magic" != "$EXT4_MAGIC" ]; then
        warn "flash verify failed: ext4 magic = $_magic"
        printf '{"ok":false,"error":"bad-magic","magic":"%s"}' "$_magic"
        return 1
    fi

    if [ -n "$_sha_want" ]; then
        _blocks=$(( (_size + CHUNK_SIZE - 1) / CHUNK_SIZE ))
        _sha_got="$(dd if="$OPT_DEV" bs=1M count="$_blocks" 2>/dev/null \
                    | head -c "$_size" | sha256sum | cut -d' ' -f1)"
        if [ "$_sha_got" != "$_sha_want" ]; then
            warn "flash verify failed: sha256 mismatch"
            printf '{"ok":false,"error":"sha-mismatch","got":"%s"}' "$_sha_got"
            return 1
        fi
    fi

    rm -f "$UPLOAD_DIR/$_id".size "$UPLOAD_DIR/$_id".done "$UPLOAD_DIR/$_id".sha 2>/dev/null
    say "flash complete: $_size bytes, magic $_magic"
    printf '{"ok":true,"written":%s,"magic":"%s"}' "$_size" "$_magic"
}

flash_abort(){
    rm -f "$UPLOAD_DIR/$1".size "$UPLOAD_DIR/$1".done "$UPLOAD_DIR/$1".sha 2>/dev/null
    printf '{"ok":true}'
}


_zero_edges(){
    dd if=/dev/zero of="$OPT_DEV" bs=1M count=4 conv=notrunc 2>/dev/null || return 1
    _sz="$(blockdev --getsize64 "$OPT_DEV" 2>/dev/null || echo 0)"
    if [ "$_sz" -gt $((8 * 1024 * 1024)) ]; then
        _blk=$(( _sz / 1048576 - 4 ))
        dd if=/dev/zero of="$OPT_DEV" bs=1M seek="$_blk" count=4 conv=notrunc 2>/dev/null || return 1
    fi
    return 0
}

opt_unmount(){
    grep -q "^[^ ]* $OPT_MNT " /proc/mounts 2>/dev/null || return 0
    umount "$OPT_MNT" 2>/dev/null || return 1
    grep -q "^[^ ]* $OPT_MNT " /proc/mounts 2>/dev/null && return 1
    return 0
}

opt_mount(){
    mkdir -p "$OPT_MNT" 2>/dev/null || return 1
    grep -q "^[^ ]* $OPT_MNT " /proc/mounts 2>/dev/null && return 0
    mount -t ext4 "$OPT_DEV" "$OPT_MNT" 2>/dev/null || return 1
    grep -q "^[^ ]* $OPT_MNT " /proc/mounts 2>/dev/null
}

ext4_magic(){
    dd if="$OPT_DEV" bs=1 skip=$EXT4_MAGIC_OFF count=2 2>/dev/null | od -An -tx1 | tr -d ' \n'
}

mkfs_ext4(){
    mkfs.ext4 -F -L "$EXT4_LABEL" -O "$EXT4_FEATURES" -E "$EXT4_OPTS" "$OPT_DEV" >/dev/null 2>&1 && return 0
    warn "mkfs rejected ext4_features ($EXT4_FEATURES), retrying without -O"
    mkfs.ext4 -F -L "$EXT4_LABEL" -E "$EXT4_OPTS" "$OPT_DEV" >/dev/null 2>&1 && return 0
    warn "mkfs rejected ext4_opts ($EXT4_OPTS), retrying with tool defaults"
    mkfs.ext4 -F -L "$EXT4_LABEL" "$OPT_DEV" >/dev/null 2>&1
}

p34_wipe_format(){
    [ -b "$OPT_DEV" ] || { printf '{"ok":false,"error":"no-device"}'; return 1; }

    if ! opt_unmount; then
        warn "cannot unmount $OPT_MNT, refusing to touch $OPT_DEV"
        printf '{"ok":false,"error":"busy","mnt":"%s"}' "$OPT_MNT"
        return 1
    fi

    say "wiping $OPT_DEV"

    if has blkdiscard; then
        blkdiscard -f "$OPT_DEV" 2>/dev/null || _zero_edges || {
            warn "wipe failed"
            printf '{"ok":false,"error":"wipe-failed"}'
            return 1
        }
    else
        _zero_edges || {
            warn "wipe failed"
            printf '{"ok":false,"error":"wipe-failed"}'
            return 1
        }
    fi

    say "formatting $OPT_DEV (label $EXT4_LABEL)"
    if ! mkfs_ext4; then
        warn "mkfs failed"
        printf '{"ok":false,"error":"mkfs-failed"}'
        return 1
    fi
    sync

    _wm="$(ext4_magic)"
    if [ "$_wm" != "$EXT4_MAGIC" ]; then
        warn "format verify failed: ext4 magic = $_wm"
        printf '{"ok":false,"error":"bad-magic","magic":"%s"}' "$_wm"
        return 1
    fi

    if ! opt_mount; then
        warn "mount after format failed"
        printf '{"ok":false,"error":"mount-failed","mnt":"%s"}' "$OPT_MNT"
        return 1
    fi

    say "wipe and format complete, mounted at $OPT_MNT"
    printf '{"ok":true,"mnt":"%s"}' "$OPT_MNT"
}
