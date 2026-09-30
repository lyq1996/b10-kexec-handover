# build-imm-initramfs

Builds a minimal ImmortalWrt initramfs kernel plus the board device tree, from a clean
upstream checkout plus a patch series.

The build applies three layers to the source tree, then reverts all of them:

1. `patch/*.patch` — the board delta plus build-time slimming, applied with `git apply`
2. `config/` — the kernel config (`config-6.18`) and the buildroot package config
   (`buildroot.config`), copied over the upstream files
3. `overlay/files` — rootfs overlay, copied to `files/`

Because everything is applied on top of a clean checkout, the source tree must be the
`immortalwrt` submodule sitting at its pinned commit; any uncommitted changes in it are
discarded when the build ends.

## Build environment

- Linux x86_64 host. Verified on Ubuntu 26.04 LTS.
- The cross toolchain is **not** provided by the host: the OpenWrt tree builds its own
  aarch64 toolchain during the first build, which takes a while.
- An ImmortalWrt/OpenWrt source tree (the `immortalwrt` submodule). `build.sh` syncs
  `config/buildroot.config` into the tree's `.config`; that config selects the
  `qualcommbe/ipq95xx` target, the `nokia_beacon-10` device, `CONFIG_TARGET_ROOTFS_INITRAMFS=y`,
  and the LuCI web UI.
- `bash`, `git`, GNU `make`, `cpio`.

## Dependencies

Verified working set on Debian/Ubuntu:

```
sudo apt-get install -y build-essential git python3 cpio rsync wget file unzip gawk \
    bzip2 patch flex bison gettext zstd bc libncurses-dev zlib1g-dev libssl-dev
```

On other distributions install the equivalents of the packages above.

## How to build

```
git submodule update --init --recursive

bash build-imm-initramfs/build.sh immortalwrt
```

The tree path is required and may be relative. Options:

```
-o <output dir>    output directory, defaults to <script dir>/dist
```

Full usage: `bash build-imm-initramfs/build.sh --help`.

## Artifacts

Written to `<script dir>/dist/` (or `-o`):

| file | what it is |
|---|---|
| `Image-initramfs` | raw arm64 kernel with the initramfs embedded |
| `image-ipq9574-nokia-beacon-10.dtb` | board device tree |
| `SHA256SUMS` | checksums of the two files above |

`build.log` next to the script holds the full `make` output.

The kernel image must stay within the 32 MiB cap and the device tree within 128 KiB; if
either limit is exceeded the build refuses to produce artifacts, rather than emitting
something that cannot be loaded.

## Prebuilt files

`dist/` ships a set of files I prebuilt myself, for reference only.
