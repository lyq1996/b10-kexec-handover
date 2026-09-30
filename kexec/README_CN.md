# kexec

kexec核心：
1. freestanding 的 aarch64 用户态工具kjump;
2. 小内核模块kexec-lite-54.ko, kexec-lite-414.ko

两者配合，在不碰引导链的前提下把正在运行的内核换掉。

`kjump` 会解析内核源（裸 arm64 `Image`、FIT，或 ELF 容器），修好设备树里的 bootargs，
sync，下线次级 CPU，装载模块，把新内核读进内存并交接。与具体板子无关，这个目录里
没有任何只对某一块板子成立的东西。

`abi/kexec-lite.h` 是模块与用户态之间 ABI 的唯一真相来源。它带 ABI 版本号、编译期
尺寸断言，两边共用同一份，所以一旦不匹配会明确报错，而不是跳进一堆垃圾。

## 编译环境

- Linux x86_64 主机。已在 Ubuntu 26.04 LTS 上验证。
- 主机上需要 aarch64 交叉工具链：`aarch64-linux-gnu-gcc`。
- 编译内核模块时：每个目标各需要一份**已配置**的内核树（4.14 和 5.4），也就是已经有
  arm64 `.config` 的树。`kjump` 本身两样都不需要。
- `bash`、GNU `make`。

## 依赖安装

Debian/Ubuntu 上已验证可用的一组：

```
sudo apt-get install -y build-essential make bc flex bison libssl-dev \
    gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

交叉编译器前缀不同的话，覆盖 `XGCC` 即可。

其他发行版请安装上表对应包。

## 如何编译

```
cd kexec

bash build.sh all          # kjump + 两个模块
bash build.sh kjump        # 只出用户态工具
bash build.sh module-54    # 只出 5.4 模块
bash build.sh module-414   # 只出 4.14 模块
bash build.sh test         # 宿主侧解析器测试，不产生产物
bash build.sh clean        # 清掉 build/ 与 dist/
```

环境变量：

| 变量 | 含义 | 默认值 |
|---|---|---|
| `XGCC` | 交叉编译器 | `aarch64-linux-gnu-gcc` |
| `KERNEL_TREE_54` | 已配置的 5.4 内核树 | 无；`module-54` / `all` 必填 |
| `KERNEL_TREE_414` | 已配置的 4.14 内核树 | 无；`module-414` / `all` 必填 |

例如：

```
XGCC=aarch64-linux-gnu-gcc \
KERNEL_TREE_54=/path/to/configured/linux-5.4 \
KERNEL_TREE_414=/path/to/configured/linux-4.14 \
bash build.sh all
```

模块编译特意加了 `-mcmodel=large`：较新的 GCC 默认用 small code model，会产生 `adrp`
重定位，而带 `CONFIG_ARM64_ERRATUM_843419` 的内核会在模块加载器里拒收这类重定位。

## 产物路径

写到 `kexec/dist/`：

| 文件 | 说明 |
|---|---|
| `kjump` | freestanding 静态 aarch64 可执行文件（无 libc） |
| `kexec-lite-54.ko` | 5.4 内核模块（真机用） |
| `kexec-lite-414.ko` | 4.14 内核模块（QEMU 实验室用） |

`kjump` 链接后会检查未定义符号，只要还有残留就直接构建失败。

`test` 目标会在 `build/test/` 下编译并运行宿主侧解析器壳；它不需要交叉编译器、也不需要
内核树，但会读几份真固件样本，路径可用 `IMG`、`FIT`、`RAW`、`DTB`、`KERNELBIN` 覆盖。

## 预编译文件
dist目录中提供了一组由我预编译的文件，仅供参考。