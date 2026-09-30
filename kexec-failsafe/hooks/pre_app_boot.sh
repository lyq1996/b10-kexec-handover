#!/bin/sh

FS_DIR=/configs/kexec/failsafe-kexec

[ -x "$FS_DIR/boot.sh" ] || exit 0
exec "$FS_DIR/boot.sh"
