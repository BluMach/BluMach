# Next classic alpha: selected machine candidates

Preparation branch, 2026-10-14. This is not a published release or a claim that
all release acceptance checks have passed. The build identifies itself as
0.1.0-alpha.3; no release asset is created by enabling these candidates.

The next preview adds catalogue visibility and new configuration selection for:

| Machine | Initial scope | Remaining acceptance |
|---|---|---|
| Olivetti PCS 286/S, TI/OLIMCU16 | BIOS 2.06, 12 MHz, 1 MiB, internal VGA, 40 MB IDE | Fresh creation, installation, disk persistence and reset regression |
| Olivetti Prodest PC 1 | BIOS 1.07, NEC V40, 256/512/640 KiB, one/two 720 KB drives | Cold reset, keyboard, floppy read/write and display checks |
| Olivetti Prodest PC 1 HD | BIOS 1.21, 640 KiB, one 720 KB drive, 20 MB XTA (667/2/32) | Repeat installation and independent HDD boot; cold/warm reset |
| Olivetti M28 | BIOS 2.12, 8 MHz, 512 KiB, 1.2 MB 5.25-inch floppy, compatible 20 MB MFM | CMOS disk type 2 setup, installation, independent HDD boot and Ctrl+Alt+Del |
| Amstrad PC5286 | 16 MHz, 1/2/4 MiB, internal 82C452; floppy, 40 MB IDE and Games Pack profiles | Confirm OS boot, HDD installation and cold/warm Setup persistence |

These reuse their existing machine-specific implementations and declarative
creation profiles. Previous canonical observations are not fresh acceptance
results on this branch. The catalogue keeps its existing partial/experimental
warnings; enabling a sheet does not certify the hardware.

The M28 profile's old 3.5-inch drive declaration is corrected to the documented
5.25-inch 1.2 MB drive. No stored user configuration is rewritten.

PC1512 is excluded from the selected historical collection. Its inherited
emulator selector entry remains available under the existing policy; this work
does not remove inherited machines. M250/M250 E, M280 and M380 remain deferred.
The 1991 PCS 286S is folded into the PCS 286/S sheet: its creation dialog offers
12 MHz (default) and an experimental 16 MHz choice with BIOS 2.06, because the
unit's own "PCS286S REL. 1.06" EPROMs are not dumped.

Known approximations remain explicit: PC1 uses a V20-based V40 instruction path
and generic PIC/PIT/DMA; PC1 HD translates its XTA DMA; M28 uses compatible
WD1003/ST-225 storage rather than established original controller identity;
PC5286 uses an in-memory BIOS checksum correction, a compatible IDE geometry
and generic AdLib for the Games Pack. No original ROM bytes are altered.

The Amstrad manufacturer and Series 5 family and Olivetti Prodest family become
visible. Creation and configuration share the catalogue visibility policy.
Tests must cover all five product/native-machine pairs in all five locales and
keep other deferred models excluded.

ROMs, BIOS, operating systems, diagnostic software and guest disks remain local
test inputs and must not enter source commits or release packages.
