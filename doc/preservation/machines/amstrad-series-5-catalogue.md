# Series 5 catalogue: PC5086 and PC5286

Editorial review, 6 November 2026. Spanish is the reference text; English,
French, Italian and Portuguese translate the same claims and qualifications.
This review adds no hardware validation or emulator changes.

PC5286 retains its existing experimental status, visibility, creation profiles,
translation keys, section sequence and entry sequence. PC5086 is a new visible
research entry using the existing schema, without a runnable catalogue profile.
The owner explicitly changed the initial hidden-entry instruction: new research
entries in this investigation are intended for the website, even before emulation.
Existing translation notices and common strings are unchanged.

## Claims and preserved evidence

Asset identifiers below refer to the local preservation manifests. Restricted
documents, photographs and firmware remain outside this repository.

| Claim or editorial change | Evidence and locator | Classification and limit |
|---|---|---|
| PC5286 history now explains professional positioning, Counterpoint and its place beside PC5086 instead of repeating hardware | Amstrad, *The new Amstrad Series 5*, August 1991, p. 2, `pc5286-brochure-1991-08-en`; combined manual, PC Guide 1-3/1-4 | Documented manufacturer positioning; no unsupported predecessor/successor or named OEM |
| PC5286 Games Pack price: GBP 899 excluding VAT, valid 1 October 1991 | Amstrad advertisement, footer and price paragraph, `pc5286-advert-1991-10-en` | Documented UK offer; not a universal launch price |
| UK design and manufacture in Taiwan, Korea or the UK | August brochure, p. 2, legal footer | Attributed to the brochure; no particular factory or board manufacturer established |
| Games Pack accessories and supplied games | September 1991 Games Pack brochure, `pc5286-games-pack-1991-09-en`, and October advertisement | Documented package, not standard sound equipment on every PC5286 |
| PC5086 has one or two 720 KB floppy drives; family table corrected | *PC5086/PC5286 User Manual*, PC Guide 1-3, PDF p. 17, `amstrad-pc5086-pc5286-user-manual-en` | Visually confirmed in primary manual. The contemporary CBR announcement says 1.44 MB; discrepancy retained, not reconciled into an invented revision |
| PC5086: 8086 at 8 MHz or 4.77 MHz through SPEED, optional 8087-2 | Same manual, PC Guide 1-3, 4-3, 7-2 | Documented; no new speed-control implementation |
| PC5086: 640 KB, expansion-card EMS, video and disk BIOS address ranges | Same manual, PC Guide 8-4, 8-6/8-7 | Documented address map; does not establish custody of every ROM |
| PC5086: VGA compatibility, 40 MB disk, serial/parallel and two 8-bit half-length ISA slots | Same manual, PC Guide 1-3 | Documented; exact disk and video chip revisions remain pending |
| PC5286: 16 MHz, 1/2/4 MB paired SIMMs, ports and optional storage | Same manual, PC Guide 1-4, 7-2, 8-9/8-10; August brochure p. 2 | Documented; the manual's 512 KB map is not treated as a sold configuration |
| PC5286 chipset and firmware identity | Banjo/FreddyV report, 18 March 2021, `pc5286-editorial-banjo-20261106`, and preserved system/VGA ROMs | Owner observation and file identity, not a schematic or new physical inspection |
| MC0128A VGA population, restricted to the photographed unit | Ikesankom repair photographs, `pc5286-photo-forum-board-4` and `pc5286-editorial-board-page-20261106`; NEC uPD41464 datasheet p. 3B-1, `pc5286-editorial-nec-d41464-20261106` | Observed packages plus documented density: 8 × 65,536 × 4 bits = 256 KiB; no assertion about every board revision |
| PC5286 prior POST/Setup/RAM/EMS/VGA checks and remaining approximations | Existing pc5286 manifest validation records and worklogs; `pc5286-scatemm-api-v1-verification`, `pc5286-bios-mode-matrix-v1-verification`, `pc5286-checksum-test-source` | State in BluMach. Summarized prior evidence, not new guest certification |
| PC5086 firmware custody and inherited implementation | `pc5086-rom-1`, `pc5086-rom-2`; `machine_xt_pc5086_init` in `src/machine/m_xt.c` at base `88f74ba30693e14b0003c656fa3ac578c3d85eed` | Hashes and source inspection only; physical provenance, video ROM and POST remain pending |
| Series 5 announcement context | CBR Staff Writer, *Amstrad to launch Series 5 PC's*, 1 September 1991, `amstrad-series5-cbr-19910901-snapshot-20261106` | Contemporary announcement, not independent confirmation that the announced launch occurred |

## Source pages

- [Amstrad PC5086/PC5286 User Manual and Series 5 brochures — ACPC archive](https://acpc.me/).
- [Amstrad PC5286 Games Pack advertisement, October 1991 — CPCrulez archive](https://cpcrulez.fr/).
- [Amstrad to launch Series 5 PC's — CBR / Tech Monitor, 1 September 1991](https://www.techmonitor.ai/technology/amstrad_to_launch_series_5_pcs).
- [Amstrad PC5286 BIOS dump — Banjo, 18 March 2021](https://banjosmods.wordpress.com/2021/03/18/amstrad-pc5286-bios-dump/).
- [PC5286 repair and board photographs — Ikesankom, Foros de Electrónica](https://www.forosdeelectronica.com/threads/valores-fuente-de-alimentaci%C3%B3n-de-amstrad-pc5286.151834/).
- [NEC uPD41464 datasheet — ReactiveMicro DRAM archive](https://downloads.reactivemicro.com/Electronics/DRAM/).
- [PC5086 firmware collection — 86Box ROMs, preserved revision](https://github.com/86Box/roms/tree/c4cd8da9034a8557896ff6a975ae58499a83bb80/machines/pc5086).

## Deliberate omissions and follow-up

No exact PC5086 sale price, named OEM, direct predecessor or successor is added
without sufficient evidence. The GBP 400 Series 5 entry price is not assigned
to a particular PC5086 configuration. PC5386SX is not promoted to a confirmed
Series 5 member or added as a machine in this change.

Neither machine gains an image: no suitable photograph or brochure with clear
redistribution permission was established in the relevant records. Existing
PC5286 media state remains unchanged. No placeholder image is invented.

Labbook candidates, kept out of the short catalogue entries: PC5286 checksum
correction and its provenance; SCAT memory/EMS maps; C&T 82C452 cursor and timing
validation; PC5086 manual/announcement floppy-capacity discrepancy.

## Validation scope

Run the library audit for pc5086 and pc5286, catalogue builder validation,
catalogue tests, source-link tests and family-history tests. A separate local
contract check compares all existing product visibility and PC5286 structural
keys/order with the base commit, and verifies that PC5086 and its translations
are included in the website export. Exact results are recorded in the PR and
canonical worklogs. This text-only review does not rebuild the GUI or execute
firmware; the final native card layout has not been visually reviewed.
