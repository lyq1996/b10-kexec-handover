#!/bin/bash
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"

LOG="$HERE/build.log"
CONFIG_DIR="$HERE/config"
OVERLAY="$HERE/overlay/files"
PATCH_DIR="$HERE/patch"
TREE=""
OUT="$HERE/dist"

IMG_CAP=33554432
MOD_DTB_MAX=131072

CONFIG_FRAG=target/linux/qualcommbe/config-6.18
CONFIG_ROOT="$CONFIG_DIR/buildroot.config"

DTB_NAME=image-ipq9574-nokia-beacon-10.dtb

log(){ echo "[$(date +%H:%M:%S)] $*"; }
die(){ log "failed: $*"; exit 1; }

usage(){
    cat <<'EOF'
usage: ./build.sh <immortalwrt source tree> [-o <output dir>]

  <immortalwrt source tree>  required. must be a git repo, checked out at the
                             upstream commit the patches in patch/ apply to.
                             Patches and overlay are applied during the build and
                             reverted by git checkout at the end.
  -o <output dir>            optional. defaults to <script dir>/dist

what the build applies, in order:
  1. patch/*.patch   board delta and build slimming, applied with git apply
  2. config/         kernel config + buildroot .config, copied over upstream files
  3. overlay/files   rootfs overlay, copied to files/

artifacts: Image-initramfs / image-ipq9574-nokia-beacon-10.dtb / SHA256SUMS
EOF
}

patched_files(){
    grep -h '^+++ b/' "$PATCH_DIR"/*.patch 2>/dev/null | sed 's|^+++ b/||' | sort -u
}

restore_patched(){
    _rp_list="$(patched_files)"
    [ -n "$_rp_list" ] || return 0
    for _rp_f in $_rp_list; do
        if git ls-files --error-unmatch "$_rp_f" >/dev/null 2>&1; then
            git checkout -- "$_rp_f" 2>/dev/null
        else
            rm -f "$_rp_f" 2>/dev/null
        fi
    done
}

apply_patches(){
    _ap_seen=0
    for _ap_p in "$PATCH_DIR"/*.patch; do
        [ -f "$_ap_p" ] || continue
        _ap_seen=1
        git apply --whitespace=nowarn "$_ap_p" \
            || die "patch failed: $(basename "$_ap_p") (tree already patched? run git checkout -- . )"
        log "applied $(basename "$_ap_p")"
    done
    [ "$_ap_seen" = 1 ] || die "no patches found in $PATCH_DIR"
}

TREE_DIRTY=0
restore_tree(){
    [ "$TREE_DIRTY" = 1 ] || return 0
    cd "$TREE" || return 0
    git checkout -- $CONFIG_FRAG \
        || log "warning: git checkout failed, tree may still have overlay applied"
    rm -rf files
    restore_patched
    TREE_DIRTY=0
    log "build tree restored"
}
trap restore_tree EXIT

kver_detect(){
    local d
    for d in "$BD"/linux-[0-9]*; do
        [ -d "$d/usr" ] || continue
        basename "$d" | sed 's/^linux-//'
        return 0
    done
    return 1
}

do_build(){
    : > "$LOG"
    log "source tree  $TREE"
    log "output       $OUT"

    [ -d "$TREE/.git" ]       || die "source tree is not a git repo: $TREE"
    [ -d "$PATCH_DIR" ]       || die "missing $PATCH_DIR"
    [ -f "$CONFIG_DIR/config-6.18" ] || die "missing $CONFIG_DIR/config-6.18"
    [ -f "$CONFIG_ROOT" ]            || die "missing $CONFIG_ROOT"
    [ -d "$OVERLAY" ]          || die "missing $OVERLAY"

    BD="$(ls -d "$TREE"/build_dir/target-*/linux-qualcommbe_ipq95xx 2>/dev/null | head -1)"
    [ -n "$BD" ] || die "build_dir not found ($TREE/build_dir/target-*/linux-qualcommbe_ipq95xx)"

    IMG_BUILT="$BD/Image-initramfs"
    DTB_BUILT="$BD/$DTB_NAME"

    rm -rf "$OUT"; mkdir -p "$OUT"

    cd "$TREE" || die "cannot enter $TREE"
    TREE_DIRTY=1

    log "applying patches"
    apply_patches

    log "applying config"
    cp -a "$CONFIG_DIR/config-6.18" "$CONFIG_FRAG"
    cp -a "$CONFIG_ROOT" .config

    log "applying rootfs overlay"
    cp -a "$OVERLAY" files
    [ -f files/etc/init.d/ro_guard ] || die "overlay apply failed ($OVERLAY incomplete?)"

    log "clean + prepare"
    make target/linux/clean   >> "$LOG" 2>&1
    make target/linux/prepare >> "$LOG" 2>&1 || die "prepare failed (patch?) see $LOG"

    log "make -j$(nproc)"
    make -j"$(nproc)" >> "$LOG" 2>&1 || die "make failed, see $LOG"

    log "verifying artifacts"
    [ -f "$IMG_BUILT" ] || die "no $IMG_BUILT produced (CONFIG_TARGET_ROOTFS_INITRAMFS not set in .config?)"
    [ -f "$DTB_BUILT" ] || die "no $DTB_BUILT produced (board DTS not built?)"

    local kver cpio size dsize
    kver="$(kver_detect)" || die "$BD/linux-<ver> not found"
    cpio="$BD/linux-$kver/usr/initramfs_data.cpio"
    [ -f "$cpio" ] || die "missing $cpio"
    cpio -it < "$cpio" 2>/dev/null | grep -q 'etc/init.d/ro_guard' \
        || die "overlay not in initramfs"

    size=$(stat -c %s "$IMG_BUILT")
    dsize=$(stat -c %s "$DTB_BUILT")
    [ "$size"  -le "$IMG_CAP" ]     || die "kernel $size B exceeds kjump IMG_CAP ($IMG_CAP)"
    [ "$dsize" -le "$MOD_DTB_MAX" ] || die "dtb $dsize B exceeds kjump MOD_DTB_MAX ($MOD_DTB_MAX)"
    log "kernel $size B / dtb $dsize B (kernel version $kver)"

    restore_tree

    log "collecting artifacts"
    cp -a "$IMG_BUILT" "$OUT/Image-initramfs"
    cp -a "$DTB_BUILT" "$OUT/$DTB_NAME"
    for pair in "$OUT/Image-initramfs:$IMG_BUILT" "$OUT/$DTB_NAME:$DTB_BUILT"; do
        [ "$(sha256sum "${pair%%:*}" | cut -d' ' -f1)" \
          = "$(sha256sum "${pair##*:}" | cut -d' ' -f1)" ] || die "checksum mismatch after copy: ${pair##*:}"
    done
    ( cd "$OUT" && sha256sum Image-initramfs "$DTB_NAME" > SHA256SUMS )
    sed 's/^/    /' "$OUT/SHA256SUMS"

    log "done"
}

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        -o|--out)  [ -n "${2:-}" ] || die "-o needs a directory argument"; OUT="$2"; shift 2 ;;
        -*)        usage >&2; die "unknown option: $1" ;;
        *)         [ -z "$TREE" ] || die "only one source tree allowed"; TREE="$1"; shift ;;
    esac
done
[ -n "$TREE" ] || { usage >&2; exit 1; }

[ -d "$TREE" ] || die "source tree does not exist: $TREE"
TREE="$(cd "$TREE" && pwd)"
mkdir -p "$OUT" || die "cannot create output dir: $OUT"
OUT="$(cd "$OUT" && pwd)"

do_build
