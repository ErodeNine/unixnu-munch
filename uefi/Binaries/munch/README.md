# Board binary overrides

Files here replace the same path under `Binaries/munch/` in the Mu-Silicium
checkout after the SM8250 reference set (alioth) is copied in. `uefi/apply.sh`
merges this tree last.

Everything below was extracted from this phone's own `xbl_b`
(`partitions-20261025` backup): the main DXE volume is a gzip guided section
(`1d301fe9-be79-4353-91c2-d23bc959ae0c`); each FFS file is a DXE_DRIVER whose
PE32 and DXEPEX sections are copied verbatim. The `.inf` wrappers stay from the
reference set.

| Driver | Reference problem on the K40S | File |
| --- | --- | --- |
| PmicDxe | OEM error right after dispatch, platform reset (`ERROR: C900000002… IO 5776232E…`) | `QcomPkg/Drivers/PmicDxe/PmicDxeLa.efi` |
| SPMIDxe | data abort (translation fault, level 3) inside SPMI when the board PmicDxe calls it | `QcomPkg/Drivers/SPMIDxe/SPMIDxe.efi` |
| QcomChargerDxe | calls PmicDxe and hits the same SPMI path; board charges at 67 W | `QcomPkg/Drivers/QcomChargerDxe/QcomChargerDxeLA.efi` |
| ChargerExDxe | paired with the charger driver | `QcomPkg/Drivers/ChargerExDxe/ChargerExDxe.efi` |
| UsbPwrCtrlDxe | VBUS/role control on the board PMIC | `QcomPkg/Drivers/UsbPwrCtrlDxe/UsbPwrCtrlDxe.efi` |
| AdcDxe | PMIC ADC channels differ per board | `QcomPkg/Drivers/AdcDxe/AdcDxe.efi` |
| TsensDxe | thermal sensor map | `QcomPkg/Drivers/TsensDxe/TsensDxe.efi` |
| LimitsDxe | board current/voltage limits | `QcomPkg/Drivers/LimitsDxe/LimitsDxe.efi` |
| ArmCpuDxe | reference driver looks for an unknown "CPU Vectors" region and asserts in the exception handler | `ArmPkg/Drivers/CpuDxe/CpuDxe.efi` |

`ButtonsDxe` is deliberately **not** replaced: the reference copy carries the
Mu-Silicium patch that maps the power key to ENTER for the UniXNU menu.

## Adding another driver

Same layout: `Binaries/munch/<Package>/Drivers/<Name>/<Alias>.efi` with the file
name the reference `.inf` expects (`DXE_DEPEX|…depex`, `PE32|…efi`).
