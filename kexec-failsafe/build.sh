#!/bin/bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(tr -d ' \n' < "$HERE/VERSION")"
KEXEC_DIR="${KEXEC_DIR:-$HERE/../kexec}"
KEXEC_DIST="${KEXEC_DIST:-$KEXEC_DIR/dist}"

NAME="kexec-failsafe-$VERSION"
DIST="$HERE/dist"
STAGE="$HERE/build/stage"
PKG="$DIST/$NAME.tar.gz"
KEXEC_BIN="kjump kexec-lite-54.ko kexec-lite-414.ko"

die(){ echo "build.sh: $*" >&2; exit 1; }
size_of(){ stat -c%s "$1"; }

echo "== kexec-failsafe $VERSION =="
echo "   source:   $HERE"
echo "   artifact: $PKG"

echo "== build kexec core =="
[ -f "$KEXEC_DIR/build.sh" ] || die "missing $KEXEC_DIR/build.sh (set KEXEC_DIR)"
bash "$KEXEC_DIR/build.sh" all || die "kexec build failed"

echo "== collect =="
[ -d "$KEXEC_DIST" ] || die "not found: $KEXEC_DIST"
for f in $KEXEC_BIN; do
    [ -f "$KEXEC_DIST/$f" ] || die "missing $KEXEC_DIST/$f"
    printf "   %-20s %8s B\n" "$f" "$(size_of "$KEXEC_DIST/$f")"
done

for f in thttpd busybox; do
    [ -f "$HERE/bin/$f" ] || die "missing $HERE/bin/$f"
    printf "   %-20s %8s B\n" "$f" "$(size_of "$HERE/bin/$f")"
done

echo "== assemble =="
rm -rf "$STAGE"
mkdir -p "$STAGE/$NAME/kexec" "$STAGE/$NAME/failsafe/bin"

for f in $KEXEC_BIN; do
    cp "$KEXEC_DIST/$f" "$STAGE/$NAME/kexec/"
done
chmod 755 "$STAGE/$NAME/kexec/kjump"

cp -a "$HERE/failsafe/." "$STAGE/$NAME/failsafe/"
cp "$HERE/bin/thttpd"  "$STAGE/$NAME/failsafe/bin/thttpd"
cp "$HERE/bin/busybox" "$STAGE/$NAME/failsafe/bin/busybox"
chmod 755 "$STAGE/$NAME/failsafe/bin/thttpd" "$STAGE/$NAME/failsafe/bin/busybox"

find "$STAGE/$NAME/failsafe" -name '*.sh' -exec chmod 755 {} +
chmod 644 "$STAGE/$NAME/failsafe/www/index.html"
chmod 755 "$STAGE/$NAME/failsafe/www/cgi-bin"/*.cgi

echo "== inject defaults =="
[ -f "$HERE/failsafe/state/defaults.json" ] || die "missing failsafe/state/defaults.json"
python3 - "$STAGE/$NAME/failsafe/www/index.html" "$HERE/failsafe/state/defaults.json" <<'PY'
import sys, re, io
html_p, json_p = sys.argv[1], sys.argv[2]
h = io.open(html_p, encoding='utf-8').read()
j = io.open(json_p, encoding='utf-8').read().strip()
new, n = re.subn(r'(<script id="kfs-defaults" type="application/json">).*?(</script>)',
                 lambda m: m.group(1) + j + m.group(2), h, flags=re.S)
if n != 1:
    sys.exit("FATAL: defaults injection point matched %d times (expected 1)" % n)
io.open(html_p, 'w', encoding='utf-8').write(new)
print("   defaults.json injected into index.html")
PY

cp "$HERE/VERSION" "$STAGE/$NAME/VERSION"

echo "== generate manifest =="
( cd "$STAGE/$NAME" && find . -type f ! -name MANIFEST.sha256 \
    | sed 's|^\./||' | LC_ALL=C sort | xargs sha256sum > MANIFEST.sha256 )
echo "   $(wc -l < "$STAGE/$NAME/MANIFEST.sha256") files"

echo "== package =="
mkdir -p "$DIST"
rm -rf "$DIST/$NAME"
tar -C "$STAGE" -czf "$PKG" "$NAME"
cp -a "$STAGE/$NAME" "$DIST/$NAME"
rm -rf "$HERE/build"

echo
echo "== done =="
echo "   pkg:    $PKG"
echo "   tree:   $DIST/$NAME"
echo "   size:   $(size_of "$PKG") B"
echo "   sha256: $(sha256sum "$PKG" | cut -d' ' -f1)"
