# 声明
本项目仍在实验阶段，除临时启动外，任何其他功能都没有验证，且100%存在问题。
如果你因使用这个测试项目，导致设备损坏，本人概不负责。

# kexec-handover

在厂商 Secure Boot 锁死的路由器上跑你自己的内核。

Secure Boot 的验签只管到 HLOS 启动那一刻。内核一旦跑起来，验签的边界就结束了。所以这个
项目不去跟签名硬碰，而是**完全不碰引导链**，在运行时用 `kexec` 把正在跑的内核换掉。不往
引导槽写任何东西。

已在 Nokia Beacon 10（Qualcomm IPQ9574）上验证。

设想的跳转逻辑：
1. `/configs/pre_app_boot.sh` 拉起`failsfae`里的`boot.sh`；
2. `boot.sh`检查当前模式，支持三种模式：`跳转新系统`、`临时留在官方系统`、`始终停留官方系统`；
3. `boot.sh`闪烁wps灯，给用户一个窗口，窗口内若按下WPS按钮，进入failsafe(一个带web界面的恢复控制台)；
4. `websafe`可刷写新系统、查看日志、执行终端命令、跳转initramfs。

## 目录结构

| 路径 | 是什么 |
|---|---|
| `kexec/` | **交接核心。** `kjump` 是一个 freestanding（无 libc）的 aarch64 用户态程序：解析内核源、修好设备树里的 bootargs、sync、下线次级 CPU、装载一个小内核模块，然后交接。此外还有那个模块本身，与具体板子无关。 |
| `build-imm-initramfs/` | **编译 initramfs 内核。** 编出一个内嵌 initramfs 的最小 ImmortalWrt 内核（另加一层 rootfs 覆盖层和一组上游文件替换），产出裸内核和板级 dtb。 |
| `kexec-failsafe/` | **Web控制台。** 一个自包含的 Web 界面，由 thttpd 在 1717 端口提供服务，后端是 shell CGI：上传内核镜像 + 设备树并跳转过去、刷写或抹除 `opt` 分区、开 shell、看日志。它的构建同时产出 kexec 核心和Web控制台。 |

每个目录下都有自己的 `README.md`，写明编译环境、依赖安装、编译步骤与产物路径。

### `immortalwrt` 是 submodule，板级 delta 是 patch

`immortalwrt/` 是一个 git submodule，钉在本项目所基于的上游提交上
（`20edd83`，`VIKINGYFY/immortalwrt`）：

```
git submodule update --init --recursive
```

构建时会用 `git apply` 打到干净的 submodule 检出上再编译。这样 delta
始终可见、可评审，而且能机械地 rebase：把 patch 系列 rebase 到新的上游提交、挪一下
submodule 指针，就完事了。

## 编译

### kexec-failsafe —— kexec 核心 + Web控制台

模块构建两份内核树，linux-4.14是开发时qemu模拟使用，但是目前代码未移除。linux-5.4是beacon10官方固件的内核版本。

```
export KERNEL_TREE_54=/path/to/configured/linux-5.4
export KERNEL_TREE_414=/path/to/configured/linux-4.14
bash kexec-failsafe/build.sh
```

在 `kexec-failsafe/dist/` 下产出：

- `kexec-failsafe-<version>.tar.gz`： 可直接部署的包
- `kexec-failsafe-<version>/`：解压后：
  - `kexec/` —— `kjump`、`kexec-lite-54.ko`、`kexec-lite-414.ko`
  - `failsafe/` —— Web控制台（`start.sh`、`lib/`、`www/`、`bin/thttpd`、`bin/busybox`）

常用环境变量：`XGCC`、`KEXEC_DIR`、`KEXEC_DIST`、`THTTPD_SRC`、`BUSYBOX_SRC`、
`SKIP_KEXEC_BUILD=1`（直接打包现成的 kexec，不重新编）。

### build-imm-initramfs —— 内核镜像 + 设备树

```
bash build-imm-initramfs/build.sh /path/to/immortalwrt
```

在 `build-imm-initramfs/dist/` 下产出：

- `Image-initramfs` —— 内嵌 initramfs 的裸 arm64 内核
- `image-ipq9574-nokia-beacon-10.dtb` —— 板级设备树
- `SHA256SUMS`

构建往源码树上的patch在结束时会全部还原。

### 单独编 kexec

```
cd kexec
bash build.sh [all|kjump|module-414|module-54|test|clean]
```

`test` 跑宿主侧的解析器测试；它不需要交叉编译器，也不需要内核树。

## 工作原理

这三块能力是正交的 —— 每一块单独拿出来都有用：

- **交接**回答的是"怎么在不碰引导链的前提下把正在跑的内核换掉？"。它是一个通用工具；
  `kexec/` 里没有任何东西是某块板子专属的。
- **板级移植**回答的是"怎么让上游 OpenWrt 支持这块板子，而且 delta 始终可见、能跟得上
  上游？"。
- **可复现构建**回答的是"别人凭什么相信你发布的镜像就是从这份源码编出来的？"。

最初的设想，是想证明一台被锁死的设备可以在不攻击这把锁的前提下真正变得开放：承认签名
破不了，只在它管不到的地方动手。这也是为什么这里没有任何一处会去写引导槽、写环境变量或
烧熔丝。

## 当前进展

- **交接在真机上可用** 已验证能跳到自制 initramfs。
- **Web控制台可用功能** 上传Initramfs内核 + 设备树并跳转；交互式终端；日志查看；红线自检。
- **仅支持部署到 `/tmp`** 不要作死部署到`/configs`。


## 局限
- **目前只有 initramfs 能引导。** 常驻的 `opt` 分区引导路径还在开发中；
- **thttpd 会在 30 秒后杀掉任何 CGI**（这是 thttpd 自身的编译期常量）；
- **Web控制台没有任何认证。** 不要把它暴露在不可信网络上；
- **只在我自己的板子上验证过。**

## 备注
- `kexec-failsafe/bin/thttpd` 是交叉编译得来，静态链接，原厂`thttpd`加载了`libsec_engine.so`，阻断了`insmod`的能力。
- `kexec-failsafe/bin/busybox` 是静态链接的 busybox（GPL-2.0）。原厂 `rootfs` 里没有 `od`、`blockdev` 和 `timeout`。

## 快速测试
1. 上传`kexec-handover/kexec-failsafe/dist/kexec-failsafe-0.1.0.tar.gz`到beacon10的`/tmp`目录；
2. 解压: `cd /tmp && tar -zxvf /tmp/kexec-failsafe-0.1.0.tar.gz`;
3. 允许1717端口: `iptables -I INPUT 1 -i br+ -p tcp --dport 1717 -j ACCEPT && /etc/init.d/firewall reload`
4. 拉起web控制台: `sh /tmp/kexec-failsafe-0.1.0/failsafe/start.sh -f`;
5. 浏览器访问： `192.168.12.1:1717`，如果你的beacon10 lan地址不是这个请自行修改；
6. 打开`临时启动` -> `上传kexec-handover/build-imm-initramfs/dist/Image-initramfs` -> 上传`kexec-handover/build-imm-initramfs/dist/image-ipq9574-nokia-beacon-10.dtb` -> 点击`上传并立即跳转`。TIPS：若开机时间过久，内存存在大量碎片则可能无法加载内核镜像，重启即可。
7. 等待30秒，验证`openwrt`已跳转，且ssh可以链接（无需密码）：`ssh -o StrictHostKeyChecking=off root@192.168.1.1`；

![guide](./guide.png)
![result](./initramfs-booted.png)