# UniXNU-munch

English | [简体中文](README.zh-CN.md)

Device repository for the **Xiaomi Redmi K40S / POCO F4** (`munch`, `22021211RC` /
`22021211RG`), Qualcomm **SM8250-AC** (Snapdragon 870, kona), for
[UniXNU](https://unixnu.com/).

This repository is the *code* half of the port. The *build* half lives in
[WuBarlynn/unixnu-munch-build](https://github.com/WuBarlynn/unixnu-munch-build):
that repository holds the GitHub Actions workflow, the release pipeline and every
image it produces. The workflow checks this repository out, so a push here is
what a build consumes.

| Repository | Role |
| --- | --- |
| [WuBarlynn/unixnu-munch](https://github.com/WuBarlynn/unixnu-munch) | this device repository: board.toml, DTS, defconfig, XNU archive recipes, UEFI platform overlay |
| [WuBarlynn/unixnu-munch-build](https://github.com/WuBarlynn/unixnu-munch-build) | build workflow, images, releases, real-device test documents |
| [FFT-infra/UniXNU](https://github.com/FFT-infra/UniXNU) | build driver (`./unixnu`), component catalog, ARM64 and OS profiles (the fork that carries `QcomXnuUsbDebug` and the SM8250 work) |
| [FFT-infra/UniXNU-sm8250-common](https://github.com/FFT-infra/UniXNU-sm8250-common) | SM8250 / SM8250-AC resources, DTSI, config fragment, XNU patches |
| [FFT-infra/uefi-alioth](https://github.com/FFT-infra/uefi-alioth) | Mu-Silicium base firmware: UniXNU app, RamDisk block driver, DarwinBoot, SM8250 platform includes |
| [FFT-infra/UniXNU-alioth](https://github.com/FFT-infra/UniXNU-alioth) | the SM8250 reference port this repository was forked from |

## Hardware basis

The K40S is an SM8250-AC board whose relevant wiring is identical to alioth
(Redmi K40 / POCO F3). Values below are read from the vendor kernel device tree
(`arch/arm64/boot/dts/vendor/qcom/kona.dtsi`, `munch-sm8250.dtsi`,
`xiaomi-sm8250-common.dtsi` of LineageOS `android_kernel_xiaomi_sm8250`):

| Item | Value |
| --- | --- |
| SoC | SM8250-AC (kona), 4× Cortex-A55 + 4× Cortex-A77 |
| RAM | 6 / 8 / 12 GiB variants, first bank at `0x80000000` |
| Display | 1080×2400 AMOLED, 120 Hz; `cont_splash_region` at `0x9c000000` size `0x2300000` |
| Storage | UFS 3.1 host at `0x1d84000`, PHY `0x1d87000` |
| Touch | FocalTech on GENI **SE4** (`qupv3_se4_spi`), 6 MHz, reset GPIO 33, IRQ GPIO 39, 10 contacts, coords `0 0 10800 24000` |
| Wireless | QCA6390 (Wi-Fi 6 / BT 5.2) |
| Buttons | `gpio-keys`: power, volume up, volume down |

The same table drives `dts/munch.dts`, `uefi/munchPkg/Library/MemoryMapLib`
and `uefi/Resources/DTBs/munch.dts`. `uefi/munchPkg/Library/MemoryMapLib`
descriptors are the SM8250 reference map; `kona.dtsi` reserves the same ranges
on munch, so no address differs from alioth.

## State

Bring-up. Inherited from `UniXNU-alioth` and renamed; nothing below has been
measured on a K40S yet.

| Area | Status | Evidence |
| --- | --- | --- |
| Configuration | built | `./unixnu defconfig/status/tree` resolve |
| UEFI firmware | built | `munchPkg` DSC/FDF, overlay in `uefi/` |
| Kernel | built + packaged | `kernel.macho` 120.1 MB, `UniXNU.efi`, `SILICIUM_UEFI.fd`, `payload.img`, `boot.img` from build-18 |
| Storage | inherited device-verified on alioth | UFS stream ids `0x0e0` / `0x4e0` |
| Display | stage 2 | stage 1 draws boot progress and panics on the kernel frame buffer; the GOP userspace backend needs a qualified candidate |
| Input | stage 2 | the FocalTech archive returns with the display stage |
| GPU / Wi-Fi / audio / power | not implemented | A650 probe only |

The K40S panel controller, touch IC revision and charger topology differ from
alioth in the Android device tree; the first three are behind the simple
framebuffer and GENI SE4 paths, so they do not block the kernel handoff, but the
touch reset GPIO (33 in the device tree, 101 in the inherited kit) must be
re-verified on hardware. See the build repository's
`docs/03-real-device-test.md`.

## Build

Run from a UniXNU checkout:

```sh
git clone --recurse-submodules https://github.com/FFT-infra/UniXNU
git clone https://github.com/WuBarlynn/unixnu-munch
git clone https://github.com/FFT-infra/uefi-alioth

./unixnu defconfig munch_defconfig -D ../unixnu-munch
./unixnu fetch
./unixnu status --json
./unixnu tree

# firmware: apply this repository's UEFI overlay first
uefi/apply.sh ../uefi-alioth
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../uefi-alioth
./unixnu build payload --unique-chip-id <hex> --trust-cache <bundle> --data-ramdisk <seed>
./unixnu bootimg --uefi ../uefi-alioth --dtb ../uefi-alioth/Resources/DTBs/munch.dtb
```

The kernel and userdata steps need macOS (`Xcode`, `hdiutil`). The build
repository's workflow runs them on a `macos-26` runner; firmware, package and
boot image run on Linux/macOS.

## Inputs

| Item | Value |
| --- | --- |
| OS profile | `ipados-17.7.11-21H461` (ADT identity overridden to `iPhone10,3` / D22 for the 20:9 panel) |
| IPSW | `iPad_Pro_HFR_17.7.11_21H461_Restore.ipsw` |
| Userdata window | first LBA `0x2a8000`, last LBA `0x3b037fa`, 4096-byte blocks — inherited, re-read from the munch GPT before a real flash |
| Firmware config / DSC | `munch` / `Platforms/Xiaomi/munchPkg/munch.dsc` |
| Firmware volume | `Build/munchPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`, base `0x9FC00000`, size `0x300000` |
| Payload volume | `MUNCHESP`, 176 MiB |

Installing replaces Android's `userdata` partition and writes `boot_a`/`boot_b`.
Read the build repository's test document before flashing anything.

## Provenance

Forked from [`FFT-infra/UniXNU-alioth`](https://github.com/FFT-infra/UniXNU-alioth)
at `c567d24ba8870a0d027d31bd6dea6d4dc945f20d`, then board identifiers renamed and
the UEFI platform added under `uefi/`. The archive recipes, kits and package
sources are that port's work; the munch-specific changes are listed in
`docs/CHANGES-from-alioth.md`.
