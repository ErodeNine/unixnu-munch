#!/usr/bin/env bash
# Apply the Redmi K40S (munch) platform onto a Mu-Silicium checkout.
#
#   uefi/apply.sh /path/to/uefi-alioth
#
# The base checkout is https://github.com/FFT-infra/uefi-alioth, the Mu-Silicium
# fork that already carries the UniXNU boot manager, the RamDisk block driver,
# DarwinBoot and the SM8250 (kona) platform includes. This overlay adds the
# munch platform and its build-time inputs:
#
#   Platforms/Xiaomi/munchPkg/       DSC, FDF, memory map, configuration map
#   Resources/Configs/munch.toml    UEFI FD base/size and Android boot image layout
#   Resources/DTBs/munch.dts/.dtb   firmware device tree appended to the boot image
#   Silicium-ACPI/.../munch/        DSDT package the DSC names (currently alioth's)
#   Binaries/munch/                 vendor DXE inputs
#
# The vendor DXE set is the SM8250 reference set shipped in Binaries/alioth.
# Until a munch dump is extracted from the retail UEFI partition, the build uses
# that copy; see docs/03-real-device-test.md, step D2, for the extraction.
set -euo pipefail

uefi=${1:?usage: uefi/apply.sh /path/to/uefi-alioth}
overlay=$(cd "$(dirname "$0")" && pwd)
[ -f "$uefi/Platforms/Xiaomi/aliothPkg/alioth.dsc" ] || {
  echo "apply.sh: $uefi is not a uefi-alioth checkout" >&2
  exit 1
}

install -d "$uefi/Platforms/Xiaomi" "$uefi/Resources/Configs" "$uefi/Resources/DTBs" \
  "$uefi/Silicium-ACPI/Platforms/Xiaomi"
rm -rf "$uefi/Platforms/Xiaomi/munchPkg"
cp -a "$overlay/munchPkg" "$uefi/Platforms/Xiaomi/munchPkg"
cp -a "$overlay/Resources/Configs/munch.toml" "$uefi/Resources/Configs/munch.toml"
cp -a "$overlay/Resources/DTBs/munch.dts" "$uefi/Resources/DTBs/munch.dts"
cp -a "$overlay/Silicium-ACPI/Platforms/Xiaomi/munch" "$uefi/Silicium-ACPI/Platforms/Xiaomi/munch"

# The preflight check (scripts/check-munch.py) requires every Binaries/munch
# input named by the DSC/FDF includes. The SM8250 reference set is byte-identical
# to what the K40S needs for display-less (simple framebuffer) bring-up.
if [ ! -d "$uefi/Binaries/munch" ]; then
  cp -a "$uefi/Binaries/alioth" "$uefi/Binaries/munch"
fi

dtc -I dts -O dtb -o "$uefi/Resources/DTBs/munch.dtb" "$uefi/Resources/DTBs/munch.dts"
echo "apply.sh: munch platform installed in $uefi"
