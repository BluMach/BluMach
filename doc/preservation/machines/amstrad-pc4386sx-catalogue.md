# Amstrad PC4386SX: catalogue evidence

Editorial review: 7 November 2026. Scope: a visible historical research entry and
Series 4 family in Spanish, English, French, Italian and Portuguese. No emulator
implementation, firmware execution or transfer of FPGA implementation results.

## Claims and preserved sources

The source identifiers below resolve in the official local preservation record,
`amstrad/pc4386sx/manifest.json`. Third-party originals remain outside Git;
public citations link to their presenting pages.

| Claim | Source and locator | Local asset |
| --- | --- | --- |
| Presented at CeBIT in March 1991 alongside ANB-386SX and ACL-386SX; European delivery was planned for early summer | [Newsbytes, 13 March 1991, Steve Gold and Geof Wheelwright](https://www.retroisle.com/amstrad/pcs/Articles/newsbyte_mar1991.php), PC4386SX and distribution paragraphs; corroborated by [Computerwoche, 22 March 1991](https://www.computerwoche.de/article/2837011/amstrad-bringt-laptop-mit-tft-display.html), Hannover report | `pc4386sx-editorial-newsbytes`, `pc4386sx-editorial-computerwoche` |
| Compact professional desktop aimed at office space constraints; Series 4 identity | [Amstrad PC4386SX brochure, March 1991](https://retrotechwiki.com/Amstrad_PC4386SX), cover and pp. 2, 4–5 | `pc4386sx-doc-2` |
| UK recommended prices of £1,499 monochrome and £1,699 colour, excluding VAT, valid on 1 September 1991 | [Amstrad advertisement in Amstrad Computer User, November 1991](https://archive.org/details/AmstradComputerUser84-1191), PDF p. 57, product box and footer; cover inspected to confirm issue | `pc4386sx-editorial-advert-199111` |
| Made in Korea; no named OEM contractor established | Brochure p. 5 footer | `pc4386sx-doc-2` |
| 80386SX at 20 MHz, 4 MB parity RAM up to 16 MB, 64 KB cache | Brochure pp. 4–5; contemporary Computerwoche report corroborates CPU, RAM and cache | `pc4386sx-doc-2`, `pc4386sx-editorial-computerwoche` |
| Two half-length 16-bit ISA slots; 80387SX-20 socket for an optional upgrade | Brochure pp. 4–5; it does not advertise an installed coprocessor as standard | `pc4386sx-doc-2` |
| 80 MB hard disk with 19 ms access; 3.5-inch 1.44 MB floppy | Brochure pp. 4–5 | `pc4386sx-doc-2` |
| 10-inch monochrome flat screen or colour Sony Trinitron, 0.26 mm pitch | Brochure p. 2 and p. 5 | `pc4386sx-doc-2` |
| VGA with EGA/CGA/MDA/Hercules compatibility; serial, bidirectional parallel, Amstrad keyboard/mouse ports; 250 × 70 × 264 mm (W × H × D) | Brochure p. 5; original dimensions are listed W × D × H and reordered here | `pc4386sx-doc-2` |
| MS-DOS 3.3, Windows 3 and Amstrad Manager offered | Brochure pp. 4–5 | `pc4386sx-doc-2` |
| Secondary record says “4086 Ko” rather than the brochure's 4 MB | [Fabien Neck / system-cfg sheet preserved by ACPC](https://acpc.me/), PDF p. 3 | `pc4386sx-doc-1` |
| Five OEM disk images plus a modified disk 1; PCem MegaPC used as an approximation | [Banjo's Mods, Series 4 system disks, 28 January 2019](https://banjosmods.wordpress.com/2019/01/28/amstrad-pc-series-4-system-disks-includes-amstrads-custom-win3-shell/), article, package README dated 10 September 2018 and supplied configuration (`model=megapc`) | `pc4386sx-editorial-media-page`, `pc4386sx-doc-3`, existing media/profile assets |
| No dedicated PC4386SX registration in the reviewed BluMach base | [BluMach machine sources at 88f74ba3](https://github.com/BluMach/BluMach/tree/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine), `machine_table.c` and `m_amstrad.c` | Shared references `amstrad-pc2000-blumach-machine-table-base`, `amstrad-pc2000-blumach-machine-code-base` in `amstrad/_shared` |

All five brochure pages were inspected visually. The magazine advertisement was
also inspected visually. The launch month means a documented presentation, not a
verified first shipment date. The prices are dated September offers reproduced
in November, not an inferred launch price. A named OEM, direct predecessor and
successor are not established and are omitted.

## Correction of earlier research

The original local note `pc4386sx-derived-1` remains preserved unchanged for
traceability. Its assertion that the DOS utility banner is an expected ROM
identifier is not supported by the recorded analysis and is superseded.

A fresh read-only FAT12 extraction observes `PC4386SX DEVICE V3.00` at file
offset `0x1BE3` of `DEVICE.COM` and image offset `0xDE9E3` in original disk 1.
The utility is 9,681 bytes, SHA-256
`0ee87f9dab8c7b98dea8f25ca05c3ffed040169b4bf52f5f4ec496707b552e8b`.
Original disk 1 has SHA-256
`a17b0b7d98b8c88f6d0e10e3aa3cac3fd9e6f638f907df5e0264ff30f9ad52e1`.
The observed text establishes a utility banner, not a BIOS version or a string
that must occur in a genuine ROM. No comparison routine was disassembled and no
software was executed. The generated observation is registered as
`pc4386sx-editorial-device-observation`.

The PCem configuration's MegaPC identity and 40 MB disk geometry describe an
approximate software setup. They do not override the PC4386SX brochure's 80 MB
specification or prove ROM interchangeability. Likewise, a software vendor
string does not identify the video chip. The earlier PLD-versus-EPROM assignment
for AMS41110–13 is unverified and is not used in the card.

## Delivery boundaries

The RAM discrepancy is retained with both sources, without inventing another
configuration. The optional coprocessor is explained in the technical entry.
No public image is added: the local brochure and photos have no established
republication permission. The record remains in research, without a dedicated
creation profile or validated boot. Original media remain unscanned and the
system BIOS is still pending acquisition with physical provenance.

Spanish is the editorial original; all four translations carry the same facts,
dates, qualifications and limitations. Existing product visibility and
translation-provenance notes are unchanged. The common “unpreserved” collection
label uses the same “no emulation profile” wording as the preceding family PRs.
