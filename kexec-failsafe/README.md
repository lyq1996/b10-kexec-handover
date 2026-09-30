# Warning ⚠️

Most of `kexec-failsafe` has not been tested; what exists today is mostly a framework.

1. Apart from **view logs** and **temporary boot**, none of the other features are
   recommended for use!!!
2. Do not install `pre_app_boot.sh`. The `opt`-partition boot path does not work yet. When
   testing, upload this kit to `/tmp` — do not persist it!!!

# kexec-failsafe

This directory contains a `Web` UI served by `thttpd` on port `1717`, backed by a set of
shell CGIs. From a browser you can:

* upload a kernel image and device tree for a temporary jump
* flash an ext4 image containing the kernel and rootfs to the `opt` partition
  (**this feature is broken**: the image does not build yet; flashing and wiping are untested)
* auto-boot the kernel and rootfs from the `opt` partition
  (**this feature is broken**: the image does not build yet; the `opt`-partition auto-boot
  mechanism is untested)
* run shell commands (partially verified)
* view logs; streaming commands are not supported (partially verified)
* view device status: Secure Boot state, env, and so on (partially verified)
* long-pressing WPS interrupts the `opt`-partition boot and brings up the web console
  (**this feature is broken**: requires `pre_app_boot.sh` to be persisted and tested)

Also: the kexec packaging step lives in this directory, so one build produces both the kexec
artifacts and the control plane.

## Build environment

- Linux x86_64 host. Verified on Ubuntu 26.04 LTS.
- Everything `../kexec` needs for its build (see `../kexec/README.md`): the aarch64 cross
  toolchain plus the two kernel trees.
- `python3`
- `bash`, `make`, `tar`.

## Dependencies

A set verified to work:

```
sudo apt-get install -y build-essential make bc flex bison libssl-dev python3 \
    gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

`bin/` ships two prebuilt aarch64 binaries, which makes the kit self-contained: `thttpd` and
a statically linked `busybox`.

## How to build

```
export KERNEL_TREE_54=/path/to/configured/linux-5.4
export KERNEL_TREE_414=/path/to/configured/linux-4.14

bash kexec-failsafe/build.sh
```

Environment variables:

| variable | meaning | default |
|---|---|---|
| `KERNEL_TREE_54` | configured 5.4 kernel tree | none, required |
| `KERNEL_TREE_414` | configured 4.14 kernel tree | none, required |
| `XGCC` | cross compiler | `aarch64-linux-gnu-gcc` |
| `KEXEC_DIR` | kexec source directory | `<script dir>/../kexec` |
| `KEXEC_DIST` | kexec artifact directory | `$KEXEC_DIR/dist` |

## Artifacts

Written to `kexec-failsafe/dist/`:

| path | what it is |
|---|---|
| `kexec-failsafe-<version>.tar.gz` | directly deployable package |
| `kexec-failsafe-<version>/` | the same tree, already unpacked |
| `kexec-failsafe-<version>/kexec/` | `kjump`, `kexec-lite-54.ko`, `kexec-lite-414.ko` |
| `kexec-failsafe-<version>/failsafe/` | control plane: `start.sh`, `lib/`, `www/`, `bin/thttpd`, `bin/busybox`, `state/defaults.json` |
| `kexec-failsafe-<version>/MANIFEST.sha256` | checksums for every file in the package |
| `kexec-failsafe-<version>/VERSION` | package version |

Unpack it anywhere on the device, then

```
sh kexec-failsafe-<version>/failsafe/start.sh
```

`failsafe/state/defaults.json` holds the default configuration. On first run it is copied to
`failsafe/state/config.conf`, and the runtime goes by the `conf` file from then on.

`hooks/pre_app_boot.sh` is the officially supported startup hook: place it at
`/configs/pre_app_boot.sh` and it runs arbitrary commands automatically at every boot.

By default `start.sh` puts the control plane in the background and returns — exactly the
behavior `hooks/pre_app_boot.sh` wants. To keep it attached to your terminal instead
(Ctrl+C tears it down), use:

```
sh kexec-failsafe-<version>/failsafe/start.sh -f
```

Other modes: `--check` (read-only self-check) and `--probe` (verify the control plane comes
up, then stop it again).

## Prebuilt files

`dist/` ships a set of files I prebuilt myself, for reference only.
