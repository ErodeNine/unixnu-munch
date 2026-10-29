# 相对 UniXNU-alioth 的改动

[English](CHANGES-from-alioth.md) | 简体中文

基线：[`FFT-infra/UniXNU-alioth`](https://github.com/FFT-infra/UniXNU-alioth) 的
`c567d24ba8870a0d027d31bd6dea6d4dc945f20d`（2026-10-28）。

## 重命名

所有板级标识按 `alioth → munch` / `Alioth → Munch` / `ALIOTH → MUNCH` 重命名：

| alioth | munch |
| --- | --- |
| `dts/alioth.dts` | `dts/munch.dts` |
| `configs/alioth_defconfig` | `configs/munch_defconfig` |
| `kits/alioth-touch/` | `kits/munch-touch/` |
| `kits/munch-touch/build-alioth-touch.py` | `build-munch-touch.py` |
| `scripts/build-alioth-touch.py` | `scripts/build-munch-touch.py` |
| `scripts/build-alioth-pcie.py` | `scripts/build-munch-pcie.py` |
| `scripts/check-alioth.py` | `scripts/check-munch.py` |
| `package/alioth_pcie.c` | `package/munch_pcie.c` |
| `kits/munch-touch/{Alioth,Munch}Rpmh*` | `MunchRpmh*` |

## 与 alioth 不同的板级取值

| 值 | alioth | munch | 来源 |
| --- | --- | --- | --- |
| 型号 | Redmi K40 `M2012K11AC` | Redmi K40S `22021211RC` | 小米型号规则 |
| SMBIOS SKU | `M2012K11AC` | `22021211RC` | `uefi/munchPkg/munch.dsc` |
| `Resources/Configs` model_count | 3 | 1 | 单一零售型号 |
| 固件 DTB `qcom,board-id` | `<0x2c 0x00>` | `<0x32 0x00>` | `munch-sm8250-overlay.dts` |
| 固件 DTB model | `xiaomi alioth` | `xiaomi munch` | 同上 |
| ADT `compatible` | `xiaomi,alioth` | `xiaomi,munch` | `dts/munch.dts` |

其余取值刻意保持一致：SoC type 2（SM8250-AC）、内存图、`cont_splash_region`
`0x9c000000+0x2300000`、1080×2400×32 帧缓冲、UFS 主机 `0x1d84000`、GENI SE4 触控
（6 MHz，reset GPIO 33，IRQ GPIO 39）、QCA6390、IPC/QUPv3 寄存器窗口。

## 新增

- `uefi/`：munch 的 Mu-Silicium 平台（`munchPkg`）、其 `Resources` 文件，以及在
  `uefi-alioth` 检出上安装覆盖层的 `apply.sh`。基础树不随仓库携带，只提交 munch 文件。
- `docs/CHANGES-from-alioth.md` 与中文版。

## 保持原样、需要 munch 实测确认

- `feeds.toml` 将 `ShengUfs` 固定到 `FFT-infra/NativeUfs-alioth`；UFS stream id
  `0x0e0` / `0x4e0` 是 SM8250 取值，K40S 使用同一主机。
- `kits/munch-touch` 的常量来自 alioth 实测。munch 厂商 DTS 的 reset 是 GPIO 33，
  而 kit 记录为 101，需在实机复核。
- `[userdata]` 的 LBA 是 alioth 的 GPT。刷写前须按 munch GPT 重读。
- `[payload] dram_size` 为 8 GiB。6 GiB 机型改为 `0x180000000`，12 GiB 改为
  `0x300000000`。

## 首个可启动镜像的调整

- `package/LegacyDisplayProbe.cpp` 通过弱声明调用 `native_gpu_probe()`。
  Adreno650 驱动树（`drivers/Adreno650`）不在本仓库，A650 archive 属于后续阶段，
  没有它镜像仍可链接并启动显示路径。
- `configs/munch_defconfig` 不再设置 `CONFIG_XNU_NATIVE_ADRENO`（及其 archive），
  `board.toml` 不再运行 `build-adreno650.py`，也不再传 `pipa-gpu=` /
  `sheng-gpu-addon=`。等 Adreno650 驱动作为 feed 源加入后一并恢复。
- 首个镜像的 `[payload] kernel_args` 关闭了三个 SMP 参数（`sheng-smp`、
  `armcpu-spi-ipi`、`armcpu-self-ipi`）、PCIe 写入步骤（`pcie=9`、
  `pcie_skip=32`）和定时重启（`sheng-autoreset=120`）。alioth 实测 SMP 会冻结，
  PCIe 只验证到只读步骤；定时重启还会抹掉首轮 bring-up 依赖的屏幕证据。

## 阶段 1：不启用显示与触控 archive

`build-gop-legacy-display.py` 需要 `build/xnu-prelink/native-owner-mobile-graphics-candidate.macho`
与 `build/display-analysis/mobile-graphics-legacy-abi.json`，本仓库没有这两份资格产物（alioth
在自己的认证流程里生成）。首个镜像因此去掉该 archive、`build-munch-touch.py`、
`component:ShengHidInput`、`build-munch-pcie.py`，关闭 `CONFIG_XNU_NATIVE_LEGACY_DISPLAY`，
并不再传触控参数。

内核仍通过 `PE_current_console` 在帧缓冲上绘制启动进度和 panic 文本
（`build-boot-progress.py`），并可用 `component:QcomXnuUsbDebug` 从 USB CDC-ACM 输出日志。
等候选内核与运行时 ABI 观测就位后，阶段 2 再恢复显示与触控。

## 根卷策略在流水线里生成

分支里只有 OS 卷的 `build-owner-os-policy.py`，没有 `build/verified-root-systemos`
的生产者，导致 `build-verified-root-probe.py --include-systemos` 无法运行，
`build-arm-driver.py NativeVirtioBlock --verified-root --verified-root-bundle` 也没有输入。
构建仓新增 `scripts/build-root-policy.py`：用同一把自动生成的 owner 密钥为两个卷签名，
并校验 RSA 签名与每一个 4096 字节块：

- OS：展开后的整盘（`/dev/rdiskN`，6,077,580,800 字节，1,483,785 块）。
- SystemOS：IPSW 成员文件本身（`044-76034-038.dmg`，3,303,014,400 字节，806,400 块）。

`board.toml` 随后构建 VerifiedRoot bundle 与多策略块传输，配置链
（`VERIFIED_ROOT_PROBE`、`OWNER_ROOT_AUDIT/TRUST`、`NATIVE_VIRTIO_BLOCK`、
owner cache/boot-manifest/union/Data 挂载）恢复开启；显示与触控仍属阶段 2。

## DXE 堆扩大到 144 MiB

换上 K40S 自己的 PmicDxe 后，固件会派发整套 munch DXE，参考配置的 48 MiB 堆不够用
（`AllocatePoolPages: fail to allocate`）。随后 SecurityStub、Runtime、Variable、
Reset、RTC、Capsule、MonotonicCounter 都没有加载，`CoreAllEfiServicesAvailable()`
返回 EFI_NOT_FOUND，DxeMain 断言（`ASSERT [DxeCore] DxeMain.c:581`）。

`munchPkg/Library/MemoryMapLib` 现在把 `DXE Heap` 映射为
`0x93000000 + 0x09000000`（144 MiB），位于 ADSP RPC 保留区与 Display Reserved 之间；
启动镜像把内核放在 0x10008000、ramdisk 放在 0x26000000，均不冲突。

## 内核步骤要跑两遍

`build-platform-power.py` 需要上一次的预链接内核作为参考
（`out/<device>/<os>/<config>/kernel/kernel.macho`）。全新 workspace 没有这个文件，
它会归档一个**空的 `sheng_pmgr_run`**，内核因此不会发布 ApplePMGR 节点，AMFI 打印
`AMFI: No PMGR?`（`ConfigurationSettings.cpp:235`）后启动停住。在第一遍产出
kernel.macho 后再执行一次 `./unixnu build kernel`，才会链接真正的 PMGR 桥。
流水线与 `scripts/build-local.sh` 都已改为两遍。

## ButtonsDxe 换用板级版本

参考版 `ButtonsDxe` 读取的是参考机型的按键，K40S 的音量键因此到不了 UniXNU 菜单。
改用 `xbl_b` 里的板级版本。确认键不受影响：`Menu.c` 同时接受
`CHAR_CARRIAGE_RETURN` 与 `SCAN_SUSPEND`，板级电源键仍可确认条目。

## 阶段 2：启用显示与触控

2026-10-29 的实机日志显示根文件系统、launchd、keybagd、usermanagerd、datamigrator 均正常，
而 SpringBoard 每次都在同一 PC 上以 `sig5 ns=2 code=5` 崩溃，`IOMFB_FDR_Loader` 退出码 1：
用户态没有帧缓冲提供者。

`build-gop-legacy-display.py`、`build-munch-touch.py`、`component:ShengHidInput`
已回到 archive 列表，`CONFIG_XNU_NATIVE_LEGACY_DISPLAY=y`，触控参数恢复。
流水线在第一遍内核后把 `kernel.macho` 连同 `.json` 放到
`build/xnu-prelink/native-owner-mobile-graphics-candidate.macho`，供显示 recipe 使用。

运行时 ABI 观测（`gdb-stop.json`）仍缺失，因此
`audit-mobile-graphics-legacy.py` 以 `UNIXNU_DISPLAY_ABI_UNQUALIFIED=1` 运行，只跳过这一项检查。
静态检查（原版 IOMobileGraphicsFamily 代码哈希、构造链、vtable 槽位、抽象槽、类大小 0x18a0）
仍然生效。
