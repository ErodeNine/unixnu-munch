# Board binary overrides

Files here replace the same path under `Binaries/munch/` in the Mu-Silicium
checkout after the SM8250 reference set (alioth) is copied in. `uefi/apply.sh`
merges this tree last.

The first file that needs replacing is the PMIC driver: on the K40S the
reference `PmicDxeLa.efi` fails right after it is dispatched and the platform
resets with

```
ERROR: C900000002:V03000007 IO 5776232E-082D-4B75-9A0E-FE1D13F7A5D9
```

`5776232E-082D-4B75-9A0E-FE1D13F7A5D9` is the `FILE_GUID` of
`QcomPkg/Drivers/PmicDxe/PmicDxeLa.inf`. Extract the driver from the K40S's own
XBL and drop it here at the matching path:

```
Binaries/munch/QcomPkg/Drivers/PmicDxe/PmicDxeLa.efi
Binaries/munch/QcomPkg/Drivers/PmicDxe/PmicDxeLa.depex   (if the dump has one)
```

Keep the `.inf` from the reference set; only the PE32 and its dependency file
change. The same layout accepts every other driver extracted from the K40S
firmware, so a full vendor set can be assembled here without touching the
upstream tree.

How to extract (Windows, from the stock `xbl` partition or `images/xbl.elf` of
the fastboot ROM):

1. `adb shell su -c 'dd if=/dev/block/by-name/xbl_a of=/sdcard/xbl.img'`
   `adb pull /sdcard/xbl.img` — or extract `images/xbl.elf` from the ROM.
2. Download UEFIExtract (UEFITool NE releases) and run
   `UEFIExtract.exe xbl.img`
3. Open the produced `xbl.img.dump/` tree and find the folder named
   `5776232E-082D-4B75-9A0E-FE1D13F7A5D9`.
4. Inside it, the PE32 image section is the driver body; save it as
   `PmicDxeLa.efi` under the path above.
5. Commit it and push, then rebuild the firmware.
