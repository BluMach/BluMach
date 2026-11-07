# PC6486SX catalogue evidence

7 November 2026. Visible historical machine and PC6000 family entries in five
languages; Spanish original. Historical research and read-only firmware
inspection only. No emulator changes, FPGA transfer or execution.

## Claims and locators

| Claim | Preserved source | Qualification |
| --- | --- | --- |
| Consumer positioning and anticipated PC6000 replacement of PC5000 | [Tilt 103, June 1992, “Amstrad : Quel avenir ?”, p. 42](https://amigaland.com/) | Contemporary report of an intended range transition, not a proven withdrawal date; `amstrad-pc6486-editorial-tilt103` |
| French price FRF 13,990 including tax; MS-DOS 5, Windows 3 and Works; Southeast Asian manufacture | [Martine Guenzet, “Amstrad joue la carte Intel”, SVM 97, September 1992, pp. 48–49](https://archive.org/details/science-et-vie-micro-097), p. 49 first and second columns | Attributed contemporary offer, not a global launch price. No named contractor; `amstrad-pc6486-editorial-svm-leaf48` |
| 20 MHz 486SX, 4 MB and 105 MB advertised in Britain | [Computers by Post, Personal Computer World, January 1993](https://worldradiohistory.com/UK/Personal-Computer-World/), PDF p. 119, Amstrad table | Dealer configuration, not proof of every shipped unit; `amstrad-pc6486-editorial-pcw199301` |
| Clock discrepancy: 16 MHz / 20 MHz / 25 MHz | Tilt p. 42 / SVM p. 49 and PCW advertisement / owner and matching BIOS identifier | Preserved with every source; no three factory revisions inferred |
| CAM33-P2, 25 MHz 486SX, 4 MB, Trident ISA and I/O cards | [dekuNukem owner repository at ca6c8117](https://github.com/dekuNukem/Vintage_Computer_ROM_Dumps/tree/ca6c811796a028179e766da04799c83cdf5622ff), PC6486SX section | Owner's specimen, not universal configuration; `pc6486sx-editorial-owner-readme` |
| P82C206 and AMI labels visible on the board | Same repository, `rom_dumps/photos/amstrad_486.jpeg` | Direct visual observation of the supplied photograph; no full chipset identification; `pc6486sx-editorial-owner-photo` |
| Physical 27C512 read with TL866II Plus, 64 KiB file | Owner's general dumping statement and PC6486SX section, plus local file | Owner provenance and verified file size/hash, not independent physical re-read; existing `pc6486sx-asset-1` |
| No dedicated BluMach machine registration in reviewed base | [BluMach machine sources at 88f74ba3](https://github.com/BluMach/BluMach/tree/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine), machine_table.c and m_amstrad.c | Source inspection, no boot test; existing shared snapshot references |

Tilt's cover and printed p. 42, PCW's cover and advertisement at PDF p. 119, and
SVM printed pp. 48–50 were visually inspected. Full Tilt and PCW PDFs were
preserved; other search hits were navigation leads, not a complete issue review.
For SVM, Archive-rendered JPEGs of the cover and pp. 48–50 are preserved, not the
whole PDF or original JP2 files. The CPCrulez transcription is retained as a
navigation source; the printed original supplies the price and manufacturing
claim. The SVM photos show LP8486, not PC6486SX, and are not used as card images.

Tilt's 16 MHz is printed in the scan. The reviewed PCW table actually says
486SX, 20 MHz and 4 MB; a web extraction reporting 386SX/25/2 MB is incorrect
for that page. No erroneous snippet is adopted as a separate configuration.
Tilt's anticipated successor range is not a claim that every PC5000 model was
already replaced. SVM's Intel manufacturing discussion concerns LP8486 and does
not make Intel the PC6486SX OEM. The named board contractor remains unestablished.

## Preserved firmware observation

Existing system image: 65,536 bytes; SHA-256
`951fad778e6b41e41fe7edb9c5628916c47d4ded4995fcbcae10e12f84176b9a`.
It matches the repository file fetched at
`ca6c811796a028179e766da04799c83cdf5622ff` byte for byte. Original bytes and
rights/security classification are unchanged. The earlier unresolved repository
revision is supplemented with this pinned comparison, not silently rewritten.

| File offset | Observed text | Limit |
| --- | --- | --- |
| 0xF0 | `CPM25P0,4,5` followed by AMI ROM BIOS copyright text | Does not prove identical hardware to every named board |
| 0x150 | `AMSTRAD plc PC6486SX25, 07/10/1992` | Literal internal identifier/date; no day/month interpretation or launch date inferred |
| 0x57E0 | `Amstrad 486SX25` | Supports the owner-attributed 25 MHz identity, not successful operation |

Generated observation: `pc6486sx-editorial-firmware-observation`. Read-only
strings and hash inspection, no disassembly or execution. The file also carries
an AMI `06/06/91` date; neither internal date establishes commercial release.
The whole-file byte sum is recorded without treating it as a valid/invalid BIOS
test, since the appropriate checksum region/algorithm was not established.

The repository has an MIT license, but it does not establish the author's rights
to distribute third-party BIOS code. Firmware remains local, rights unknown and
unscanned. The board photograph's publication scope was not independently
established; it remains local and no new catalogue image is added.

## Implementation outlook

| Area | Assessment | Missing evidence |
| --- | --- | --- |
| System BIOS | Preserved, not yet validated | Independent re-read, security review and boot |
| CPU/bus | Compatible generic 486SX/ISA starting point | Specific clock, timing and board revision |
| Chipset/memory, reset, interrupt/DMA/timers | Unknown/blocker | Full component identification, board map and behaviour; photographed P82C206 alone is insufficient |
| Keyboard/RTC/NVRAM | Unknown/blocker | Controller identity, firmware coverage and configuration storage |
| Video | Unknown/blocker | Exact Trident controller and attributable ROM |
| Floppy, disk and ports | Unknown/blocker | I/O-card controllers, wiring and address maps |

Outcome: viable only after named hardware research and device/validation work.
No full machine model or compatibility with another Amstrad BIOS is claimed.
No FPGA implementation/tests validate BluMach here. Longer board mapping and BIOS
analysis belong in a future Labbook article, not the concise card.

The new entry keeps status research and has no creation profile. Existing cards,
visibility, schema, technical block order and translation notices are retained.
The common collection label matches the preceding family PRs. Catalogue tests,
source-link tests, family-history tests, website export and preservation audits
are recorded in the PR and library closure. No native-layout or boot review.
