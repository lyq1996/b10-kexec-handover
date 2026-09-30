# 注意⚠️
`kexec-failsafe`的大部分功能都没有经过测试，目前仅搭建了一个框架。
1. 除`查看日志`、`临时启动`功能以外，其他功能不建议使用！！！
2. 不要安装`pre_app_boot.sh`，当前未跑通`opt`分区引导，测试时强烈建议上传本套件到`/tmp`目录，不要持久化！！！

# kexec-failsafe
本目录包含一个 `Web` 界面，由 `thttpd` 在 `1717` 端口提供服务，后端是一组 shell CGI。
在浏览器里可以：
* 上传内核镜像与设备树临时跳转
* 将包含内核和rootfs的ext4镜像刷写到`opt`分区（`功能是坏的`，镜像未构建成功、未测试刷写和抹除）
* 自动启动`opt`分区的内核和rootfs（`功能是坏的`，镜像未构建成功、未测试自动启动`opt`分区机制）
* 执行shell命令（部分验证可用）
* 查看日志，不支持流式命令（部分验证可用）
* 查看设备状态：安全启动、env等（部分验证可用）
* 长按WPS支持中断`opt`分区引导并拉起web控制台（`功能是坏的`，需要`pre_app_boot.sh`持久化并测试）

另外：kexec的打包步骤也归这个目录，所以一次构建同时产出 kexec 的产物和控制面。

## 编译环境

- Linux x86_64 主机。已在 Ubuntu 26.04 LTS 上验证。
- ../kexec编译需要的东西全都要（见 `../kexec/README.md`）：aarch64 交叉工具链，以及2个内核树。
- `python3`
- `bash`、 `make`、`tar`。

## 依赖安装
已验证可用的一组：

```
sudo apt-get install -y build-essential make bc flex bison libssl-dev python3 \
    gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
```

`bin/` 里预置了两个 aarch64 二进制，自包含：`thttpd` 和一份静态链接的`busybox`。

## 如何编译

```
export KERNEL_TREE_54=/path/to/configured/linux-5.4
export KERNEL_TREE_414=/path/to/configured/linux-4.14

bash kexec-failsafe/build.sh
```

环境变量：

| 变量 | 含义 | 默认值 |
|---|---|---|
| `KERNEL_TREE_54` | 已配置的 5.4 内核树 | 无，必填 |
| `KERNEL_TREE_414` | 已配置的 4.14 内核树 | 无，必填 |
| `XGCC` | 交叉编译器 | `aarch64-linux-gnu-gcc` |
| `KEXEC_DIR` | kexec 源码目录 | `<脚本目录>/../kexec` |
| `KEXEC_DIST` | kexec 产物目录 | `$KEXEC_DIR/dist` |

## 产物路径

写到 `kexec-failsafe/dist/`：

| 路径 | 说明 |
|---|---|
| `kexec-failsafe-<版本>.tar.gz` | 可直接部署的包 |
| `kexec-failsafe-<版本>/` | 同一棵树，已解开 |
| `kexec-failsafe-<版本>/kexec/` | `kjump`、`kexec-lite-54.ko`、`kexec-lite-414.ko` |
| `kexec-failsafe-<版本>/failsafe/` | 控制面：`start.sh`、`lib/`、`www/`、`bin/thttpd`、`bin/busybox`、`state/defaults.json` |
| `kexec-failsafe-<版本>/MANIFEST.sha256` | 包内每个文件的校验和 |
| `kexec-failsafe-<版本>/VERSION` | 包版本号 |

在设备上解到任意位置，然后

```
sh kexec-failsafe-<版本>/failsafe/start.sh
```

`failsafe/state/defaults.json` 是默认配置，首次运行时它会被复制成 `failsafe/state/config.conf`，运行时以`conf`为准。

`hooks/pre_app_boot.sh`是官方支持的启动脚本，将其放置到`/configs/pre_app_boot.sh`即可开机自动执行任意命令。

`start.sh` 默认把控制面放到后台然后返回，`hooks/pre_app_boot.sh`要的就是这个行为。如果想让它挂在你的终端上（Ctrl+C 就能收掉），用：

```
sh kexec-failsafe-<版本>/failsafe/start.sh -f
```

其他模式：`--check`（只读自检）、`--probe`（验证控制面起得来，再把它停掉）。

## 预编译文件
dist目录中提供了一组由我预编译的文件，仅供参考。