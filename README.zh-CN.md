# UniXNU-munch

[English](README.md) | 简体中文

**Xiaomi Redmi K40S / POCO F4**（`munch`，`22021211RC` / `22021211RG`）的 UniXNU 设备仓库。
SoC 为 **SM8250-AC**（骁龙 870，kona）。

本仓库是移植的**代码半边**。**构建半边**在
[WuBarlynn/unixnu-munch-build](https://github.com/WuBarlynn/unixnu-munch-build)：
那里存放 GitHub Actions 流水线、发布流程和全部镜像。流水线会检出本仓库，所以向这里推送就是构建的输入。

| 仓库 | 作用 |
| --- | --- |
| [WuBarlynn/unixnu-munch](https://github.com/WuBarlynn/unixnu-munch) | 本设备仓库：board.toml、DTS、defconfig、XNU archive 配方、UEFI 平台覆盖层 |
| [WuBarlynn/unixnu-munch-build](https://github.com/WuBarlynn/unixnu-munch-build) | 构建流水线、镜像、Release、实机测试文档 |
| [FFT-infra/UniXNU](https://github.com/FFT-infra/UniXNU) | 构建驱动 `./unixnu`、组件目录、ARM64 与 OS profile（含 `QcomXnuUsbDebug` 与 SM8250 支持的分支） |
| [FFT-infra/UniXNU-sm8250-common](https://github.com/FFT-infra/UniXNU-sm8250-common) | SM8250 / SM8250-AC 公共资源、DTSI、内核配置片段、XNU 补丁 |
| [FFT-infra/uefi-alioth](https://github.com/FFT-infra/uefi-alioth) | Mu-Silicium 基础固件：UniXNU 应用、RamDisk 驱动、DarwinBoot、SM8250 平台 |
| [FFT-infra/UniXNU-alioth](https://github.com/FFT-infra/UniXNU-alioth) | 本仓库派生自的 SM8250 参考移植 |

## 硬件依据

K40S 是 SM8250-AC 机型，启动链所需外设与 alioth（Redmi K40 / POCO F3）一致。数据来自厂商内核设备树
（LineageOS `android_kernel_xiaomi_sm8250` 的 `kona.dtsi`、`munch-sm8250.dtsi`、`xiaomi-sm8250-common.dtsi`）：

| 项目 | 值 |
| --- | --- |
| SoC | SM8250-AC（kona），4×Cortex-A55 + 4×Cortex-A77 |
| 内存 | 6 / 8 / 12 GiB，首 bank 在 `0x80000000` |
| 显示 | 1080×2400 AMOLED 120 Hz；`cont_splash_region` = `0x9c000000` + `0x2300000` |
| 存储 | UFS 3.1 控制器 `0x1d84000`，PHY `0x1d87000` |
| 触控 | FocalTech，GENI **SE4**（`qupv3_se4_spi`），6 MHz，reset GPIO 33，IRQ GPIO 39，10 点，坐标 `0 0 10800 24000` |
| 无线 | QCA6390（Wi-Fi 6 / BT 5.2） |
| 按键 | `gpio-keys`：电源、音量加、音量减 |

同一组数据同时驱动 `dts/munch.dts`、`uefi/munchPkg/Library/MemoryMapLib` 与 `uefi/Resources/DTBs/munch.dts`。
内存描述符采用 SM8250 参考图；`kona.dtsi` 在 munch 上保留了相同的区域，因此与 alioth 没有地址差异。

## 状态

Bring-up 阶段。内容继承自 `UniXNU-alioth` 并完成改名；以下各项均尚未在 K40S 上实测。

| 区域 | 状态 | 依据 |
| --- | --- | --- |
| 配置 | 已可构建 | `./unixnu defconfig/status/tree` 解析通过 |
| UEFI 固件 | 已可构建 | `munchPkg` DSC/FDF，覆盖层在 `uefi/` |
| 内核 | 已构建并打包 | build-18 的 `kernel.macho` 120.1 MB、`UniXNU.efi`、`SILICIUM_UEFI.fd`、`payload.img`、`boot.img` |
| 存储 | 继承 alioth 的 device-verified | UFS stream `0x0e0` / `0x4e0` |
| 显示 | 阶段 2 | 阶段 1 只在内核帧缓冲上绘制启动进度与 panic；GOP 用户态后端需要资格候选内核 |
| 输入 | 阶段 2 | FocalTech archive 随显示阶段一起恢复 |
| GPU / Wi-Fi / 音频 / 电源 | 未实现 | 仅 A650 探针代码 |

K40S 的面板控制器、触控 IC 批次和充电拓扑与 alioth 不同；前两者位于简易帧缓冲与 GENI SE4 路径之后，
不阻塞内核交接，但触控复位脚（设备树为 33，继承的 kit 记录为 101）必须在实机上确认。
详见构建仓的 `docs/03-real-device-test.md`。

## 构建

在 UniXNU 检出目录中执行：

```sh
git clone --recurse-submodules https://github.com/FFT-infra/UniXNU
git clone https://github.com/WuBarlynn/unixnu-munch
git clone https://github.com/FFT-infra/uefi-alioth

./unixnu defconfig munch_defconfig -D ../unixnu-munch
./unixnu fetch
./unixnu status --json
./unixnu tree

# 固件：先应用本仓库的 UEFI 覆盖层
uefi/apply.sh ../uefi-alioth
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-alioth
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-alioth --dtb ../uefi-alioth/Resources/DTBs/munch.dtb
```

内核与 userdata 步骤需要 macOS（`Xcode`、`hdiutil`）。构建仓的流水线在 `macos-26` runner 上执行；
固件、package 与 boot image 可在 Linux/macOS 完成。

## 输入

| 项 | 值 |
| --- | --- |
| OS profile | `ipados-17.7.11-21H461`（ADT 身份覆写为 `iPhone10,3` / D22 以匹配 20:9 面板） |
| IPSW | `iPad_Pro_HFR_17.7.11_21H461_Restore.ipsw` |
| userdata 窗口 | 起始 LBA `0x2a8000`，结束 LBA `0x3b037fa`，4096 字节块——继承值，实机刷写前必须按 munch GPT 重新读取 |
| 固件 config / DSC | `munch` / `Platforms/Xiaomi/munchPkg/munch.dsc` |
| 固件卷 | `Build/munchPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`，base `0x9FC00000`，size `0x300000` |
| Payload 卷 | `MUNCHESP`，176 MiB |

安装会替换 Android 的 `userdata` 分区并写入 `boot_a`/`boot_b`。刷写前请先阅读构建仓的实机测试文档。

## 来源

派生自 [`FFT-infra/UniXNU-alioth`](https://github.com/FFT-infra/UniXNU-alioth) 的
`c567d24ba8870a0d027d31bd6dea6d4dc945f20d`，随后重命名板级标识并在 `uefi/` 下加入 UEFI 平台。
archive 配方、kits 与 package 源码均为该移植的成果；munch 相关改动见
`docs/CHANGES-from-alioth.md`。
