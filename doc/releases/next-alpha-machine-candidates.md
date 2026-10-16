# Next classic alpha: selected machine candidates

Preparation branch, 2026-10-16. This is not a published release or a claim that
all release acceptance checks have passed. The build identifies itself as
0.1.0-alpha.4; no release asset is created by enabling these candidates.

The next preview adds catalogue visibility and new configuration selection for:

| Machine | Initial scope | Remaining acceptance |
|---|---|---|
| TriGem SX386M (KS-II) | AMI DG2X-6080-020491-KB, 16 MHz, 4 MiB, external PVGA1A, 40 MB IDE type 17 | Hard disk from the sheet, low-speed mode, System+Video shadow, RAM populations, EMS |
| Olivetti M250 | BIOS PERB 1.03, 8 MHz, 1/2 MiB, integrated OVC, Conner IDE types 4, 1, 3 and 2 | POST, Setup and System Test after the memory/video/CMOS review; RTC; disk installation |
| Olivetti M250 E | BIOS PDSF 1.06, 12 MHz, integrated OVC, same Conner types | Complete validation: POST, Setup, System Test, soft reset, RTC, disk |
| Olivetti M280 | BIOS 2.16, 1 MiB, external PVGA1A, GO723 (types 23/30) or GO717 (types 6/7) | Extended KBC NVRAM for SETUP, DOS boot, hard disk |
| Olivetti M380 / M380 C | BIOS 1.09, 16 MHz, 4 MiB with 384 KiB dedicated memory, GO717 (types 6, 7, 12, 13) | Warm reset after SETUP, Resident Diagnostics ports, register 0 and register 2 bit 7 |

These reuse existing machine implementations, extended where their BIOS
disassembly showed missing behaviour: the Olivetti GO714/GO717/GO723
controllers, the M250 port 69h memory model and video BIOS, the extended 8742
protocol with NVRAM and dedicated memory on the M380, and the SX386M low-speed
mode. Previous canonical observations are not fresh acceptance results on this
branch. The catalogue keeps its partial/experimental warnings; enabling a sheet
does not certify the hardware.

No original ROM bytes are altered. The M380 BIOS has no Setup in ROM and its
own user disk is not recovered; the M280 System Test 2.02 SETUP is the
documented workaround. The M380 C NEW stays disabled because its GO470/481
video is not modelled.

The TriGem manufacturer and SX386 family become visible; the Olivetti Prodest
family sheet gains its history and model table. PC1512 stays excluded from the
selected historical collection; its inherited emulator selector entry remains
available under the existing policy. The M300 range and PCS 1992-1993 models
are deferred to a later phase.

Tests must cover the new product/native-machine pairs in all five locales and
keep other deferred models excluded.

ROMs, BIOS, operating systems, diagnostic software and guest disks remain local
test inputs and must not enter source commits or release packages.
