#!/bin/sh
set -e
cd "$(dirname "$0")/.."
CC=${CC:-gcc}
B=build/test
mkdir -p $B

BEACON=${BEACON:-$HOME/beacon10}
IMG=${IMG:-$BEACON/images/p24-0HLOS.img}
FIT=${FIT:-$BEACON/work/hlos-extract/wrapper.dtb}
RAW=${RAW:-$BEACON/work/hlos-extract/kernel.bin}
KERNELBIN=${KERNELBIN:-$BEACON/work/hlos-extract/kernel.bin}
if [ -z "$DTB" ]; then
    for f in "$BEACON"/work/hlos-extract/*.dtb; do
        if [ "$(stat -c%s "$f")" -lt 200000 ]; then DTB=$f; break; fi
    done
fi

echo "== build native test shells"
$CC -O2 -Wall -Wextra -Ikjump -o $B/test_container \
    test/test_container.c kjump/container.c kjump/fdt.c kjump/lzma.c kjump/lzma-sdk/LzmaDec.c
$CC -O2 -Wall -Wextra -Ikjump -o $B/test_fdt test/test_fdt.c kjump/fdt.c
$CC -O2 -Wall -Wextra -Ikjump -o $B/test_lzma \
    test/test_lzma.c kjump/lzma.c kjump/lzma-sdk/LzmaDec.c
echo "build ok"

echo
echo "== [1] real firmware container: ELF32 -> FIT -> LZMA full chain (md5 hard criterion)"
$B/test_container "$IMG" --dump $B/p24-kernel.img
if cmp -s $B/p24-kernel.img "$KERNELBIN"; then
    echo "PASS: decompressed result is byte-identical to kernel.bin"
else
    echo "FAIL: decompressed result differs from kernel.bin"; exit 1
fi
md5sum $B/p24-kernel.img "$KERNELBIN"

echo
echo "== [2] bare FIT image / bare Image (same real firmware)"
$B/test_container "$FIT" --dump $B/fit-kernel.img
cmp -s $B/fit-kernel.img "$KERNELBIN" && echo "PASS: bare FIT image extracted identically" \
    || { echo "FAIL: bare FIT image"; exit 1; }
$B/test_container "$RAW" --dump $B/raw-kernel.img
cmp -s $B/raw-kernel.img "$RAW" && echo "PASS: bare Image byte-identical" \
    || { echo "FAIL: bare Image"; exit 1; }

echo
echo "== [3] LZMA property vector: python random data round-trip"
python3 - <<'EOF'
import lzma, random
random.seed(1)
data = bytes(random.getrandbits(8) for _ in range(300000)) + b"0123456789" * 1000
with open("build/test/vec1.lzma", "wb") as f:
    f.write(lzma.compress(data, format=lzma.FORMAT_ALONE))
with open("build/test/vec1.raw", "wb") as f:
    f.write(data)
EOF
$B/test_lzma $B/vec1.lzma $B/vec1.out
cmp -s $B/vec1.out $B/vec1.raw && echo "PASS: LZMA random vector round-trip identical" \
    || { echo "FAIL: LZMA vector"; exit 1; }

echo
echo "== [4] negative vectors: must be rejected"
head -c 100 $B/vec1.lzma > $B/vec_trunc.lzma
if $B/test_lzma $B/vec_trunc.lzma $B/x.out 2>/dev/null; then
    echo "FAIL: truncated stream accepted"; exit 1
else echo "PASS: truncated stream rejected"; fi
printf '\377\377\377\377\000\000\000\000' > $B/vec_badprops.lzma
cat $B/vec1.lzma >> $B/vec_badprops.lzma
if $B/test_lzma $B/vec_badprops.lzma $B/x.out 2>/dev/null; then
    echo "FAIL: bad props accepted"; exit 1
else echo "PASS: bad props rejected"; fi
head -c 4096 /dev/urandom > $B/garbage.bin
if $B/test_container $B/garbage.bin 2>/dev/null; then
    echo "FAIL: garbage source accepted"; exit 1
else echo "PASS: garbage source rejected"; fi

echo
echo "== [5] FDT bootargs: three rewrite paths + self-consistent readback"
echo "-- source: $DTB"
$B/test_fdt "$DTB" --get
$B/test_fdt "$DTB" --set "console=ttyMSM0,115200" --dump $B/dtb1.dtb
$B/test_fdt $B/dtb1.dtb --get | grep -q "console=ttyMSM0,115200" \
    && echo "PASS: readback consistent after first write" || { echo "FAIL: first write readback"; exit 1; }
LONG="root=/dev/mmcblk0p26 rootfstype=ext4 rootwait console=ttyMSM0,115200 earlycon=qup_uart,0x88200000 init=/sbin/init loglevel=7 extra_flag_for_grow_padding=0123456789abcdef"
$B/test_fdt $B/dtb1.dtb --set "$LONG" --dump $B/dtb2.dtb
$B/test_fdt $B/dtb2.dtb --get | grep -q "mmcblk0p26" \
    && echo "PASS: readback consistent on grow path" || { echo "FAIL: grow readback"; exit 1; }
$B/test_fdt $B/dtb2.dtb --set "console=ttyS0" --dump $B/dtb3.dtb
$B/test_fdt $B/dtb3.dtb --get | grep -q "ttyS0" \
    && echo "PASS: readback consistent on in-place shrink" || { echo "FAIL: shrink readback"; exit 1; }
$B/test_fdt $B/dtb3.dtb --append "root=/dev/mmcblk0p27" --dump $B/dtb4.dtb
$B/test_fdt $B/dtb4.dtb --get | grep -q "ttyS0 root=/dev/mmcblk0p27" \
    && echo "PASS: readback consistent on append path" || { echo "FAIL: append readback"; exit 1; }
if command -v fdtdump >/dev/null 2>&1; then
    if fdtdump $B/dtb4.dtb > /dev/null 2>&1; then
        echo "PASS: fdtdump external validation passed"
        fdtdump $B/dtb4.dtb | grep -A1 bootargs | head -4
    else
        echo "FAIL: fdtdump rejected the rewritten dtb"; exit 1
    fi
else
    echo "note: fdtdump unavailable, skipping external validation (self-consistent readback already passed)"
fi

echo
echo "ALL HOST TESTS PASSED"
