# Amstrad PC3000: historical catalogue evidence

Editorial review: 7 November 2026. Models: PC3086, PC3286, PC3386SX.
Source-code baseline: BluMach `88f74ba30693e14b0003c656fa3ac578c3d85eed`.
Three visible research cards and one family, Spanish original with equivalent
English, French, Italian and Portuguese translations. Visibility does not assert
emulation readiness. No executable catalogue profiles, emulator changes or media
are introduced.

## Preserved sources

Originals, exact acquisition URLs, sizes, SHA-256 values and distribution rights
are registered in the official preservation library's `amstrad/_shared/manifest.json`.
The dossier is also preserved there as asset
`amstrad-pc3000-catalogue-dossier-20261107`.
Only this original synthesis enters the repository; third-party scans, photographs,
forum HTML and firmware remain outside Git.

| ID | Source and locator | Public page |
| --- | --- | --- |
| B3086 | Amstrad, Generation 3 PC3086 leaflet, August 1990, pp. 1–2; asset `amstrad-pc3000-3086-brochure-20261107` | [ACPC archive](https://acpc.me/) |
| B3286 | Amstrad, Generation 3 PC3286 leaflet, August 1990, pp. 1–2; asset `amstrad-pc3000-3286-brochure-20261107` | [ACPC archive](https://acpc.me/) |
| B3386 | Amstrad, Generation 3 PC3386SX leaflet, August 1990, pp. 1–2; asset `amstrad-pc3000-3386sx-brochure-20261107` | [ACPC archive](https://acpc.me/) |
| U | Amstrad, User Manual and MS-DOS Guide, second edition, first published and revised 1990; printed pp. 1-2–1-4, 4-18, 8-7 (PDF pp. 16–18, 72, 133); asset `amstrad-pc3000-user-manual-20261107` | [ACPC archive](https://acpc.me/) |
| F | Phylante and reply, Amstrad PC3286 Bios dump, 28 March 2016, posts 104755–104756; asset `amstrad-pc3000-pc3286-dump-testimony-20261107` | [Bannister forum](https://forums.bannister.org/ubbthreads.php?Number=104755&ubb=showthreaded) |
| C | BluMach machine_table.c and m_amstrad.c at the baseline above; existing preserved assets `amstrad-pc2000-blumach-machine-table-base` and `amstrad-pc2000-blumach-machine-code-base`, rechecked against this checkout | [Pinned machine sources](https://github.com/BluMach/BluMach/tree/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine) |
| R | DOS ROM BIOS Collection; original ZIP and selected unchanged members in PC3086 record | [Internet Archive collection](https://archive.org/details/dos_rom_bios_collection) |

The acquired U scan is English, despite `[FRA]` in its original filename. Its
copyright page explicitly identifies the second edition, 1990. The brochure
date is visible in each copyright footer; it is evidence of a dated offer, not
an independently established announcement or first shipment date.

Supplementary preserved leads: John Elliott's Amstrad PC models page, Cliff
Lawson's AT support notes, the Centre for Computing History's 1990 SM-PC3000
service-manual catalogue record, and RetroTechWiki's leaflet/manual indexes.
These do not replace the acquired manufacturer documents. The museum entry is
a catalogue record, not acquisition of the service manual itself.

## Claims and decisions

| Claim | Evidence and locator | Classification / decision |
| --- | --- | --- |
| Generation 3 existed as a marketed three-model range by August 1990 | B3086/B3286/B3386, dated footer and p. 1 positioning; U copyright and specifications | Documented. Use 1990; do not label August as exact launch month. |
| PC3086 aimed at home/small business, PC3286 at expandable professional work, PC3386SX at demanding standalone/network use | Respective leaflets p. 1 | Documented manufacturer positioning, attributed to Amstrad; no sales-success claim. |
| UK design; manufacture in Taiwan, Korea or UK | Each leaflet p. 2 footer | Documented generic statement. No contractor or factory assigned to a specific machine or batch. No unsupported OEM name. |
| PC3086: 8086/8 MHz, 640 KB, 8087 socket, optional 30 MB disk, 720 KB or 360 KB floppies, four ISA 8-bit slots | B3086 pp. 1–2; U p. 1-2 | Documented. Three full-length and one half-length slot. Memory expansion requires an ISA expanded-memory card. |
| PC3286: 286/16 MHz, 1 MB marketed, optional 40 MB IDE, 1.44 MB or 1.2 MB floppy, 80287 socket | B3286 pp. 1–2; U p. 1-3 | Documented. Optional disk distinguished from PC3386SX standard disk. |
| PC3286 memory sizes disagree | B3286 p. 2 lists 2/4/8/10/16 MB expansions; U p. 8-7 lists 512/640 KB and 1/2/2.5/4/8/16 MB configurations | Documented discrepancy. Both lists shown; no invented jumper setting or physical confirmation for 10 MB. Hardware overview describes the leaflet's 1–16 MB range, not a continuous list of validated configurations. |
| PC3386SX speed disagrees | B3386 pp. 1–2 says 20 MHz; U p. 1-3 says 16 MHz; U p. 4-18 DEVICE SPEED example uses 20 MHz | Documented discrepancy, checked on scans. Show 16 / 20 MHz with explanation; do not invent two commercial variants or silently correct the manual. |
| PC3386SX: 1 MB standard, 4/16 MB upgrades, four identical SIMMs | B3386 pp. 1–2; U p. 8-7 | Documented. U's introductory generic minimum of 512 KB for both ATs conflicts with its PC3386SX-only table restriction to 1/4/16 MB; retain that qualification. |
| PC3386SX: standard 40 MB IDE and 1.44 MB floppy, 80387SX socket | B3386 pp. 1–2 | Documented. |
| AT models: three ISA 16-bit and two ISA 8-bit slots, one of each half-length | B3286/B3386 p. 2 and U p. 1-3 | Documented, mutually consistent. |
| All three: VGA compatibility modes, 25-pin serial/parallel, chassis 425 × 135 × 390 mm, monitor choices, MS-DOS 3.3 | Each leaflet p. 2; U pp. 1-2–1-3 | Documented. Dimensions follow W × H × D. U names MS-DOS v3.30A; the cards retain the leaflet's family-level 3.3. |
| PC3086 firmware custody | Canonical PC3086 manifest and current hash audit | Observed files, not demonstrated boot or physical dump authenticity. fc00.bin 16 KiB; c000.bin 32 KiB. R supplies controller candidate 8 KiB and keyboard candidate 2 KiB. |
| PC3286 dump exists as a report | F, original post | Documented owner testimony: DEBUG dump reported; chip reading planned. No acquired binary or confirmed chip-dump delivery. |
| PC3386SX local firmware gap | Reviewed canonical records and existing research | Observed local research state only. Synchronization remains in progress; no assertion that a dump does not exist elsewhere or historically. |
| PC3086 dedicated code, AT models no dedicated registration at baseline | C: machine_table.c `pc3086`; m_amstrad.c `machine_pc3086_init` and `AMS_PC3086` | Observed source. PVGA1A and XTA WD XT150 paths exist; not proof of successful execution or historical fidelity. |

## Firmware and implementation boundary

PC3086 firmware hashes are rechecked without executing the bytes. Unknown rights
and not-scanned security status are retained. The loader's 65536-byte span is
not evidence that the preserved fc00.bin is 64 KiB; its actual size is 16 KiB.
Controller/keyboard candidates are not represented as additional system BIOS
revisions. Neither common documentation nor related BIOS version numbers establish
ROM interchangeability between PC3286 and PC3386SX.

This editorial task performs no new emulation viability certification. PC3086
has existing code and firmware for further research, but reproducible boot and
peripheral validation remain pending. PC3286 and PC3386SX are documentation-only
catalogue entries at this baseline, pending locally identified firmware and
dedicated hardware models. All three remain `research` and have no catalogue
creation profile.

BIOS observations, datasheets and historical hardware facts may inform BluMach.
FPGA core implementations, solutions, test outcomes and implementation status
do not become BluMach implementation evidence. No FPGA results or emulator
changes are transferred here.

## Open research and publication decisions

- Exact first-sale dates, dated launch prices, direct predecessor/successor
  assignments and named OEMs are not established by the consulted sources;
  they are omitted instead of inferred from numbering.
- Brochures contain photographs but reserve rights. No photograph or scan has
  been added to catalogue media; publishable photographs remain pending.
- Obtain and inspect the complete SM-PC3000 service manual for board-level
  details; the museum's catalogue listing alone cannot establish those details.
- PC3286: acquire the reported dump and establish physical chip provenance.
  PC3386SX: locate an attributable dump and inspect physical clock/RAM variants.
- Labbook candidates: compare the PC3000 memory-option tables and PagePool/EMS
  controls; investigate the PC3386SX clock discrepancy against real boards.
  Neither topic is expanded into an oversized catalogue entry.

## Validation scope

Run the three machine audits, shared-library integrity check, scoped index
refresh, catalogue builder, catalogue/source-link/family-history tests and release
identity check. A separate closure record contains actual outcomes. Existing
cards, their visibility, translation-provenance notices and non-editorial
preservation state are retained. No native build, visual UI review or firmware
execution is claimed.
