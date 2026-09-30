#!/bin/bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DIST="$HERE/dist"
BUILD="$HERE/build"
ABI="$HERE/abi"

XGCC="${XGCC:-aarch64-linux-gnu-gcc}"
KERNEL_TREE_414="${KERNEL_TREE_414:-}"
KERNEL_TREE_54="${KERNEL_TREE_54:-}"

die(){ echo "build.sh: $*" >&2; exit 1; }
mkdir -p "$DIST"

usage(){
    cat <<'EOF'
usage: bash build.sh [all|kjump|module-414|module-54|test|clean]

  all          default; builds all three artifacts
  kjump        userspace handover tool only (freestanding, no libc)
  module-414   kernel module for 4.14 only
  module-54    kernel module for 5.4 only
  test         host-side offline parser tests, no artifacts
  clean        remove build/ and dist/

environment:
  XGCC             cross compiler, default: aarch64-linux-gnu-gcc
  KERNEL_TREE_414  configured 4.14 kernel tree, required by module-414
  KERNEL_TREE_54   configured 5.4 kernel tree, required by module-54

artifacts land in dist/:
  kjump  kexec-lite-414.ko  kexec-lite-54.ko
EOF
}

build_kjump(){
    echo "== kjump =="
    command -v "$XGCC" >/dev/null 2>&1 || die "missing cross compiler: $XGCC"
    "$XGCC" -ffreestanding -nostdlib -static -O2 -Wall -Wextra -fno-builtin \
        -I"$HERE/kjump" -I"$ABI" \
        -Wl,-z,max-page-size=4096 -o "$DIST/kjump" \
        "$HERE/kjump/kjump.c" "$HERE/kjump/common.c" "$HERE/kjump/fdt.c" \
        "$HERE/kjump/lzma.c" "$HERE/kjump/lzma-sdk/LzmaDec.c" "$HERE/kjump/container.c"
    if nm "$DIST/kjump" | grep -q ' U '; then
        nm "$DIST/kjump" | grep ' U '
        die "kjump has undefined symbols"
    fi
    echo "   -> $DIST/kjump ($(stat -c%s "$DIST/kjump") B)"
}

build_module(){
    local tree="$1" dir="$2" out="$3"
    echo "== $out =="
    [ -n "$tree" ]         || die "kernel tree not set (export KERNEL_TREE_54 / KERNEL_TREE_414)"
    [ -d "$tree" ]         || die "kernel tree does not exist: $tree"
    [ -f "$tree/.config" ] || die "kernel tree not configured: $tree/.config missing"
    rm -rf "$dir"; mkdir -p "$dir"
    cp "$HERE/module/kexec-lite-main.c" "$HERE/module/kexec-tramp.S" "$dir/"
    cp "$ABI/kexec-lite.h" "$dir/"
    cat > "$dir/Makefile" <<'EOF'
obj-m := kexec-lite.o
kexec-lite-y := kexec-lite-main.o kexec-tramp.o
# New gcc defaults to the small code model, so module code has adrp
# relocations; kernels with CONFIG_ARM64_ERRATUM_843419 reject
# R_AARCH64_ADR_PREL_PG_HI21 in the module loader. -mcmodel=large
# uses movz/movk + literal pool, accepted by 4.14 and 5.4 loaders.
ccflags-y += -mcmodel=large
EOF
    make -C "$tree" ARCH=arm64 CROSS_COMPILE="${XGCC%-gcc}-" M="$dir" modules
    cp "$dir/kexec-lite.ko" "$DIST/$out"
    echo "   -> $DIST/$out ($(stat -c%s "$DIST/$out") B)"
}

run_tests(){ sh "$HERE/test/run-tests.sh"; }

case "${1:-all}" in
    kjump)      build_kjump ;;
    module-414) build_module "$KERNEL_TREE_414" "$BUILD/mod414" kexec-lite-414.ko ;;
    module-54)  build_module "$KERNEL_TREE_54"  "$BUILD/mod54"  kexec-lite-54.ko ;;
    test)       run_tests ;;
    clean)      rm -rf "$BUILD" "$DIST"; echo "cleaned build/ and dist/" ;;
    all)        build_kjump
                build_module "$KERNEL_TREE_414" "$BUILD/mod414" kexec-lite-414.ko
                build_module "$KERNEL_TREE_54"  "$BUILD/mod54"  kexec-lite-54.ko ;;
    *)          usage; exit 1 ;;
esac
echo "build.sh: done"
