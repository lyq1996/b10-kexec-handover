# build-imm-initramfs

从一份干净的上游 + 一组补丁，编译出最小的 ImmortalWrt initramfs 内核和板级设备树。

构建会往源码树上按顺序贴三层东西，结束后全部还原：

1. `patch/*.patch` —— 板级 delta，用 `git apply` 打上
2. `trim/` —— 构建期瘦身覆盖，直接盖掉上游同名文件
3. `overlay/files` —— rootfs 覆盖层，拷成 `files/`

因为都是在干净上游之上叠加，源码树必须是 `immortalwrt` 子模块钉住的那个提交，树里未提交
的改动会在结束时被丢弃。

## 编译环境

- Linux x86_64 主机。已在 Ubuntu 26.04 LTS 上验证。
- 交叉工具链**不由主机提供**：OpenWrt 树会在第一次构建时自己编出 aarch64 工具链，
  这一步比较慢。
- 一份已配置的 ImmortalWrt/OpenWrt 树（`immortalwrt` 子模块），其 `.config` 选中
  `qualcommbe/ipq95xx` 目标和 `nokia_beacon-10` 设备，并打开
  `CONFIG_TARGET_ROOTFS_INITRAMFS=y`。
- `bash`、`git`、GNU `make`、`cpio`。

## 依赖安装

Debian/Ubuntu 上已验证可用的一组：

```
sudo apt-get install -y build-essential git python3 cpio rsync wget file unzip gawk \
    bzip2 patch flex bison gettext zstd bc libncurses-dev zlib1g-dev libssl-dev
```

其他发行版请安装上表对应包。

## 如何编译

```
git submodule update --init --recursive

bash build-imm-initramfs/build.sh immortalwrt
```

源码树路径必填，可以是相对路径。可选参数：

```
-o <产物目录>    产物输出目录，默认 <脚本目录>/dist
```

完整用法：`bash build-imm-initramfs/build.sh --help`。

## 产物路径

写到 `<脚本目录>/dist/`（或用 `-o` 指定）：

| 文件 | 说明 |
|---|---|
| `Image-initramfs` | 裸 arm64 内核，initramfs 已内嵌 |
| `image-ipq9574-nokia-beacon-10.dtb` | 板级设备树 |
| `SHA256SUMS` | 上述两个文件的校验和 |

`build.log` 是 `make` 的完整输出。

内核镜像必须 <= 32 MiB 上限、设备树必须 <= 128 KiB 内；
超限时构建会直接拒绝出产物，而不是产出一个加载不了的文件。

## 预编译文件
dist中提供了一组由我预编译的文件，仅供参考。