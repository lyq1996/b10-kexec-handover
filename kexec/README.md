# kexec

The kexec core:

1. `kjump`, a freestanding aarch64 userspace tool;
2. two small kernel modules, `kexec-lite-54.ko` and `kexec-lite-414.ko`.

Together they replace the running kernel without touching the boot chain.

`kjump` parses the kernel source (a bare arm64 `Image`, a FIT, or an ELF container), fixes
up the bootargs in the device tree, syncs, takes the secondary CPUs offline, loads the
module, reads the new kernel into memory, and hands over. It is board-agnostic: nothing in
this directory holds only for one particular board.

`abi/kexec-lite.h` is the single source of truth for the ABI between the module and
userspace. It carries an ABI version and compile-time size assertions, and both sides share
the same file, so a mismatch fails loudly instead of jumping into a pile of garbage.

## Build environment

- Linux x86_64 host. Verified on Ubuntu 26.04 LTS.
- An aarch64 cross toolchain on the host: `aarch64-linux-gnu-gcc`.
- For the kernel modules: one **configured** kernel tree per target (4.14 and 5.4), i.e. a
  tree that already has an arm64 `.config`. `kjump` itself needs neither.
- `bash`, GNU `make`.

## Dependencies

A set verified to work on Debian/Ubuntu:

```
sudo apt-get install -y build-essential make bc flex bison libssl-dev \
    gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

If your cross compiler prefix differs, override `XGCC`.

On other distributions, install the equivalents of the packages above.

## How to build

```
cd kexec

bash build.sh all          # kjump + both modules
bash build.sh kjump        # userspace tool only
bash build.sh module-54    # 5.4 module only
bash build.sh module-414   # 4.14 module only
bash build.sh test         # host-side parser tests, no artifacts
bash build.sh clean        # remove build/ and dist/
```

Environment variables:

| variable | meaning | default |
|---|---|---|
| `XGCC` | cross compiler | `aarch64-linux-gnu-gcc` |
| `KERNEL_TREE_54` | configured 5.4 kernel tree | none; required by `module-54` / `all` |
| `KERNEL_TREE_414` | configured 4.14 kernel tree | none; required by `module-414` / `all` |

For example:

```
XGCC=aarch64-linux-gnu-gcc \
KERNEL_TREE_54=/path/to/configured/linux-5.4 \
KERNEL_TREE_414=/path/to/configured/linux-4.14 \
bash build.sh all
```

The modules are deliberately built with `-mcmodel=large`: newer GCC defaults to the small
code model, which emits `adrp` relocations, and a kernel built with
`CONFIG_ARM64_ERRATUM_843419` rejects those relocations in its module loader.

## Artifacts

Written to `kexec/dist/`:

| file | what it is |
|---|---|
| `kjump` | freestanding static aarch64 executable (no libc) |
| `kexec-lite-54.ko` | kernel module for 5.4 (real hardware) |
| `kexec-lite-414.ko` | kernel module for 4.14 (QEMU lab) |

After linking, `kjump` is checked for undefined symbols and the build fails outright if any
remain.

The `test` target builds and runs the host-side parser harness under `build/test/`; it needs
neither a cross compiler nor a kernel tree, but it reads a few real firmware samples whose
paths can be overridden with `IMG`, `FIT`, `RAW`, `DTB`, `KERNELBIN`.

## Prebuilt files

`dist/` ships a set of files I prebuilt myself, for reference only.
