# Changes from UniXNU-alioth

English | [简体中文](CHANGES-from-alioth.zh-CN.md)

Base: [`FFT-infra/UniXNU-alioth`](https://github.com/FFT-infra/UniXNU-alioth)
commit `c567d24ba8870a0d027d31bd6dea6d4dc945f20d` (2026-10-28).

## Renames

Every board identifier was renamed `alioth → munch` / `Alioth → Munch` /
`ALIOTH → MUNCH`:

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

## Board values that differ from alioth

| Value | alioth | munch | Source |
| --- | --- | --- | --- |
| Model | Redmi K40 `M2012K11AC` | Redmi K40S `22021211RC` | Xiaomi model number |
| SMBIOS SKU | `M2012K11AC` | `22021211RC` | `uefi/munchPkg/munch.dsc` |
| `Resources/Configs` model_count | 3 | 1 | one retail model |
| Firmware DTB `qcom,board-id` | `<0x2c 0x00>` | `<0x32 0x00>` | `munch-sm8250-overlay.dts` |
| Firmware DTB model | `xiaomi alioth` | `xiaomi munch` | same |
| ADT `compatible` | `xiaomi,alioth` | `xiaomi,munch` | `dts/munch.dts` |

Everything else is deliberately identical: SoC type 2 (SM8250-AC), memory map,
`cont_splash_region` `0x9c000000+0x2300000`, 1080×2400×32 framebuffer, UFS host
`0x1d84000`, GENI SE4 touch at 6 MHz with reset GPIO 33 / IRQ GPIO 39, QCA6390,
IPC/QUPv3 register windows.

## Added

- `uefi/` — the munch Mu-Silicium platform (`munchPkg`) plus its `Resources`
  files and an `apply.sh` that installs the overlay on a `uefi-alioth` checkout.
  The base tree is not vendored; only the munch files are.
- `docs/CHANGES-from-alioth.md` — this file.

## Kept as-is, needs munch verification

- `feeds.toml` pins `ShengUfs` to `FFT-infra/NativeUfs-alioth`; the UFS stream ids
  `0x0e0` / `0x4e0` are SM8250 values and the K40S uses the same host.
- `kits/munch-touch` constants come from alioth measurements. The vendor munch DTS
  puts reset on GPIO 33, while the kit records 101; re-check on hardware.
- `[userdata]` LBAs are alioth's GPT. Re-read them from the munch GPT before flashing.
- `[payload] dram_size` is 8 GiB. Change to `0x180000000` (6 GiB) or
  `0x300000000` (12 GiB) for those variants.

## First-bootable changes

- `package/LegacyDisplayProbe.cpp` calls `native_gpu_probe()` through a weak
  declaration. The Adreno650 driver tree (`drivers/Adreno650`) is not part of this
  repository and the A650 archive is a later bring-up stage, so an image built
  without it still links and boots the display path.
- `configs/munch_defconfig` no longer sets `CONFIG_XNU_NATIVE_ADRENO` (and its
  archive), and `board.toml` no longer runs `build-adreno650.py` or passes
  `pipa-gpu=` / `sheng-gpu-addon=`. Re-enable them together when the Adreno650
  driver is added as a feed source.
- `[payload] kernel_args` for the first image disables the three SMP arguments
  (`sheng-smp`, `armcpu-spi-ipi`, `armcpu-self-ipi`), the PCIe write steps
  (`pcie=9`, `pcie_skip=32`) and the timed reset (`sheng-autoreset=120`).
  alioth measured SMP freezes and the PCIe steps only reached read-only; a timed
  reset would also erase the panel evidence a first bring-up depends on.

## Stage 1: no display or touch archive

`build-gop-legacy-display.py` needs `build/xnu-prelink/native-owner-mobile-graphics-candidate.macho`
and a recorded `build/display-analysis/mobile-graphics-legacy-abi.json`, neither of which this
repository ships; alioth staged them from its own qualification runs. The first image therefore
drops that archive, `build-munch-touch.py`, `component:ShengHidInput` and `build-munch-pcie.py`,
sets `CONFIG_XNU_NATIVE_LEGACY_DISPLAY` off, and stops passing the touch arguments.

The kernel still draws boot progress and panic text on the frame buffer through
`PE_current_console` (`build-boot-progress.py`), and `component:QcomXnuUsbDebug` carries the log
over USB CDC-ACM. Stage 2 re-enables display and touch once the candidate and the runtime ABI
observation are staged.

## Owner root policies are generated in the pipeline

The fork ships `build-owner-os-policy.py` for the OS volume but no producer for
`build/verified-root-systemos`, so `build-verified-root-probe.py --include-systemos`
could never run and `build-arm-driver.py NativeVirtioBlock --verified-root
--verified-root-bundle` had no input. The build repository now carries
`scripts/build-root-policy.py`, which signs both volumes under one generated owner
key and verifies the RSA signature and every 4096-byte block:

- OS: the expanded whole disk (`/dev/rdiskN`, 6,077,580,800 bytes, 1,483,785 blocks).
- SystemOS: the IPSW member itself (`044-76034-038.dmg`, 3,303,014,400 bytes, 806,400 blocks).

`board.toml` then builds the VerifiedRoot bundle and the multi-policy block
transport, and the config chain (`VERIFIED_ROOT_PROBE`, `OWNER_ROOT_AUDIT/TRUST`,
`NATIVE_VIRTIO_BLOCK`, the owner cache/boot-manifest/union/Data mounts) is on again.
Display and touch remain stage 2.

## DXE heap enlarged to 144 MiB

With the K40S PmicDxe in place the firmware dispatches the whole munch DXE set
and the reference 48 MiB heap runs out (`AllocatePoolPages: fail to allocate`).
SecurityStub, Runtime, Variable, Reset, RTC, Capsule and MonotonicCounter then
never load, `CoreAllEfiServicesAvailable()` returns EFI_NOT_FOUND and DxeMain
asserts (`ASSERT [DxeCore] DxeMain.c:581`).

`munchPkg/Library/MemoryMapLib` now maps `DXE Heap 0x93000000 + 0x09000000`
(144 MiB) instead of `0x98900000 + 0x03300000`. The window sits between the
ADSP RPC reservation and Display Reserved; the boot image places the kernel at
0x10008000 and the ramdisk at 0x26000000, so nothing overlaps.
