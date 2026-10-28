# Changes from UniXNU-alioth

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
