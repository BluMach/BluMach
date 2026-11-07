# Amstrad professional 8486 family — 2026-11-07

Two confirmed commercial lines, ES8486 and LP8486, receive separate visible
research cards. They are not one generic motherboard. SX25/DX33 remain
documented CPU/configuration options within each line. No third duplicate
“PC8486” machine is created: Lawson's general 8486 designation belongs to this
research scope, while the contemporary sources identify ES and LP explicitly.

Spanish is the original; English, French, Italian and Portuguese preserve its
facts and qualifications. Existing schema, keys, visibility and translation
notices remain unchanged. No emulator changes, creation profiles, firmware
execution or FPGA implementation evidence. Base reviewed: BluMach
88f74ba30693e14b0003c656fa3ac578c3d85eed.

## Sources preserved in the official library

The source IDs below belong to `amstrad/_shared`. Existing originals are reused
by ID and verified SHA-256, not copied into new machine records. No third-party
original, firmware or publication-uncleared photograph is included in Git.

| Source ID | Citation and consulted location |
| --- | --- |
| `amstrad-pc6486-editorial-tilt103` | [Tilt 103, June 1992](https://amigaland.com/), cover and p. 42, “Amstrad : Quel avenir ?”. Full PDF already preserved; page inspected visually. |
| `amstrad-pc6486-editorial-svm-leaf47`, `-svm-leaf48`, `-svm-leaf49`; `amstrad-pc8486-editorial-svm-leaf50` | [Martine Guenzet, SVM 97, September 1992](https://archive.org/details/science-et-vie-micro-097), “Amstrad joue la carte Intel”, printed pp. 48–51. Archive-rendered JPEG scans, visually inspected. Existing cover/metadata establish issue identity. |
| `amstrad-pc8486-editorial-svm-leaf51` | Same issue, printed p. 52, acquired/read as article continuation; no additional machine specifications taken from it. This is a page scan, not a full newly acquired magazine. |
| `amstrad-pc6486-editorial-pcw199301` | [Personal Computer World, January 1993](https://worldradiohistory.com/UK/Personal-Computer-World/), Admincure advertisement, printed p. 493 / PDF p. 503. Full PDF already preserved; page inspected visually. |
| `amstrad-pc5386-editorial-lawson-general` | [Cliff Lawson, General notes on Amstrad PCs](https://www.freetimeweb.nl/home/computer/alt/lawsons-amstrad-computer_site/web.ukonline.co.uk/cliff.lawson/general.htm), 8486 row: 80486, SVGA, AT. Primary support text preserved previously; broad identity only. |
| `amstrad-pc2000-blumach-machine-table-base` | [BluMach 88f74ba3, machine_table.c](https://github.com/BluMach/BluMach/tree/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine), read-only registration inspection. Preserved source snapshot equals this checkout. |

## Claims, discrepancies and decisions

| Claim | Evidence and locator | Inclusion |
| --- | --- | --- |
| 8000 professional successor to generation 3000, Amstrad/Intel cooperation | Documented: Tilt p. 42, June 1992 | Family chronology is the announced positioning, not proof of a precise replacement/withdrawal day. SVM's September test and January dealer offer establish later concrete evidence. |
| ES is ISA; LP is EISA with SCSI and network integration | Documented: Tilt p. 42; SVM pp. 49–50; PCW p. 493 | Separate machine cards; no component transfer between lines. |
| Intel-made professional range; LP system unit made in Ireland under Amstrad specifications | Documented contemporary attribution: SVM pp. 48–49; joint development corroborated by Tilt p. 42 | LP history attributes the manufacturing statement to SVM and excludes monitor/keyboard. ES history states the collaboration/design claim; its factory is not independently established. No retail Intel-board identity or generic BIOS compatibility inferred. |
| CPU discrepancy | Tilt p. 42: 486DX/33 or **386SX/25**. SVM pp. 49/51 and PCW p. 493: **486SX/25**, 486DX/33 | Both sources explicit in all languages. Hardware table uses the later tested/advertised 486 configurations. Do not invent a confirmed 386 commercial revision or silently correct Tilt. |
| ES 4 MB to 40 MB, five 16-bit ISA slots | Documented: Admincure PCW p. 493 | Attributed technical text. SIMM topology and controller map still missing. |
| LP 4 MB to 128 MB; optional 64/128 KB Turbocache; two long-card positions | Documented: SVM p. 50; RAM corroborated by PCW p. 493 | Two long-card positions do not prove only two slots in total. |
| LP integrated IDE/SCSI/Ethernet; SCSI II and RJ-45/AUI 10BASE-T | Documented: SVM p. 50; PCW p. 493 | Interface functions documented; no exact chip IDs or register maps claimed. |
| LP 105/240 MB versus dealer 120/240 MB | Documented: SVM p. 50 and PCW p. 493 | Explicit discrepancy in all locales. No inferred country/board revision explanation. ES card uses dealer's diskless/120/240 MB options, without importing LP's tested disk. |
| ES SX25 £795; LP SX25 £1,195 | Documented: PCW p. 493, January 1993 | Dealer offers, without disk or monitor, VAT excluded. Not launch prices. DOS5/Windows3.1/mouse included. Headings contain abbreviated/irregular “EPS 486” and “LP846”; not additional models. |
| LP SX25 test configuration FRF25,602 excluding tax, 4 MB/105 MB/SVGA/1.44 MB floppy | Observed by contemporary reviewer: SVM p. 51 | Different package from the diskless UK dealer price; do not compare as a direct price reduction. |
| 8486 SVGA/AT/80486 identity | Documented: Lawson row, corroborating contemporary 486 family | No precise video controller or VRAM inferred. |

The June announcement and September review are independent magazine evidence;
the CPCrulez transcription of SVM is not an independent corroboration and is not
required by these cards. Search leads in modern lists that still question 8486
existence are superseded by these contemporary sources. Exact launch day,
withdrawal date, ES factory, full part numbers and publication-cleared photos
remain open. Photographs in the magazine are evidence only, not public media.

## Firmware, implementation and viability

No attributable ES8486/LP8486 firmware or dedicated technical manual is registered
in the inspected official library. This describes the inspected state, not
historical nonexistence or completion of incoming synchronization. No ROM was
downloaded or executed in this round. A general Intel origin cannot establish
board identity or permission to reuse a generic BIOS.

The reviewed BluMach machine table has no ES8486/LP8486/8486 entry. No current
PCem/86Box support claim is made from that local observation. This phase does
not implement either machine and does not use FPGA core work as evidence.

| Component | ES8486 | LP8486 |
| --- | --- | --- |
| CPU/bus | 486SX/DX and ISA documented; reusable CPU capability, board timing pending | 486SX/DX and EISA documented; bus model/fidelity review pending |
| Firmware | Unknown/blocker: attributable system/video dump | Unknown/blocker: attributable system/video/option-ROM coverage |
| Chipset/memory | Capacity and slots documented; controller/topology unknown | Capacity/cache options documented; controller/topology unknown |
| IRQ/DMA/timers/reset | Unknown/blocker: technical documentation | Unknown/blocker: technical documentation |
| Keyboard/RTC/NVRAM | Unknown/blocker | Unknown/blocker |
| Video | SVGA family-level evidence; chip/ROM unknown | SVGA test documented; chip/ROM unknown |
| Floppy/disk | Dealer disk configurations; controllers unknown | Tested floppy and IDE/SCSI functions; controller chips unknown |
| Ports/network | Integrated LP SCSI/network omitted; remaining map unknown | Ethernet interfaces documented; chips/register map unknown |

Conservative outcome for both: documentation only at present; implementation
requires attributable firmware, a service manual or complete board photographs,
component mapping and boot validation. First follow-ups: exact ES/LP board labels,
BIOS chip photographs/dumps and Intel OEM technical documentation. Detailed
controller comparisons belong in a later Labbook investigation, not longer cards.

Validation and PR closure are recorded in the canonical library worklogs. New
cards are visible under the owner's instruction; existing visibility is preserved.
