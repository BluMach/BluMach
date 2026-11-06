# Amstrad PC2000: historical catalogue evidence

Editorial review: 2026-11-06. Models: PC2086, PC2286 and PC2386.
Inspected BluMach base: `88f74ba30693e14b0003c656fa3ac578c3d85eed`.
Spanish is the reference text; English, French, Italian and Portuguese carry
the same claims, qualifications and source locators.

## Scope and evidence boundary

Three visible historical research cards and one family, using the existing
catalogue schema and block order. No executable catalogue profiles, emulator
changes or emulated/not-emulated filter. Existing products, their visibility
and translation-provenance notices remain unchanged.

BIOS-analysis findings, datasheets and historical/hardware information may be
used regardless of which research effort first located them. FPGA core code,
implemented solutions, core-test results and implementation status are not
evidence of BluMach implementation or validation. The intended implementation
direction remains BluMach to FPGA. The PC2086 library record's existing FPGA
work is retained separately and is not overwritten or reclassified.

The sole shared wording correction is `collection.unpreserved`: the runtime
uses it for a missing emulation platform, not for missing firmware. It now says
"No emulation profile" in all five languages, matching the PPC family PR.

## Bibliography and preserved assets

All originals remain in the official local library with SHA-256, size,
provenance and distribution metadata. Public links identify presenting pages
or the nearest archive section, never third-party download files.

| ID | Source and locator | Library record / asset |
| --- | --- | --- |
| B | [Amstrad PC2000 Series](https://acpc.me/), April 1989, printed pp. 3, 5, 7 and back cover (PDF pages 5, 7, 9, 12) | `amstrad/_shared`, `amstrad-pc2000-brochure-198904` |
| P | [Tarifs gamme PC2000](https://acpc.me/), indicative French prices dated 1 November 1988, one page | `amstrad/_shared`, `amstrad-pc2000-prices-fr-19881101` |
| C | [Amstrad kommt mit PC-Serie 2000](https://www.computerwoche.de/article/2793331/amstrad-kommt-mit-pc-serie-2000.html), Computerwoche, 30 September 1988 | `amstrad/_shared`, `amstrad-pc2000-announcement-19880930` |
| D | [Amstrad Plc, ESAM Volume I](https://www.computerhistory.org/collections/), Dataquest, September 1989, report 0004839, pp. 9–10; CHM collection file identifier 102723407-05-01, PDF pages 198–199 | `amstrad/_shared`, `amstrad-pc2000-dataquest-198909` |
| M2086 | [Amstrad PC2086 S/D/30 Service Manual](https://retrotechwiki.com/Amstrad_PC_2086_Service_Manual), printed p. 2 | `amstrad/pc2086`, `pc2086-service-manual-en` |
| M2386 | [Amstrad PC2386 Service Manual](https://archive.org/details/amstrad-pc-2386-service-manual), printed pp. 2, 19, 42, 44 | `amstrad/pc2386`, `pc2386-service-manual` |
| F2286 | [VCF Amstrad PC 2286, page 4](https://forum.vcfed.org/index.php?threads/amstrad-pc-2286.1238386/page-4#post-1312785) and [page 7](https://forum.vcfed.org/index.php?threads/amstrad-pc-2286.1238386/page-7#post-1518132), 2023 and 2026 owner attachments | `amstrad/pc2286`, `pc2286-evidence-1`, `pc2286-evidence-2`, `pc2286-acquired-10` through `-16`, `pc2286-hex-bin-149` through `-152` |
| F2386 | [PC 2386](https://archive.org/details/pc-2386), uploader-attributed v1.11 set | `amstrad/pc2386`, `pc2386-v111-zip`, four `pc2386-40189-2-bin` through `pc2386-40192-2-bin`, `pc2386-system-v111-64k-bin` |
| S | [BluMach source at 88f74ba3](https://github.com/BluMach/BluMach/tree/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine), `machine_table.c` and `m_amstrad.c` | `amstrad/_shared`, `amstrad-pc2000-blumach-machine-table-base`, `amstrad-pc2000-blumach-machine-code-base` |
| R | Byte comparison of preserved PC2286/PC2386 files, 6 November 2026 | `amstrad/_shared`, `amstrad-pc2000-rom-identity-20261106` |

B is a 12-page scan dated **April 1989 on its back cover**, not the separate
four-page 1988 product guide catalogued by the Centre for Computing History.
The acquisition also retains that museum catalogue page, the ACPC index and
Alan Sugar's retrospective [PC2000 account](https://amstrad.com/product/pc2000-series/).
The retrospective names Orion in Korea during rework/production restart, but
does not establish an exclusive manufacturer or assign factories to batches.
It is retained as a research lead; its recall narrative and causal claims are
not added to these concise cards.

The CHM collections landing page returned HTTP 403 to the web reader; the
original report was acquired and its relevant scanned pages inspected locally.
This is not a claim to have read the whole 1,225-page report collection.

## Claim-to-source decisions

| Claim | Evidence and locator | Decision |
| --- | --- | --- |
| September 1988 announcement, professional market, entry/middle/flagship positions | Documented: C; D p. 9; B pp. 3, 5, 7 | Include as announcement, not exact first-sale date in every country. |
| PC2086 standalone/network role | Documented: B p. 3 | Include; no claim of built-in network hardware. |
| French indicative prices: 6,590 / 12,890 / 34,190 francs excluding VAT | Documented: P, respectively PC2086 SD 12 MD / PC2286 DD 12 MD / PC2386 HD 12 MD | Include date, French market, 12-inch monochrome monitor and drive configuration. Not universal launch prices. |
| UK design and Korean assembly | Manufacturer statement: B back cover | Attribute explicitly. The contractor is not named there. |
| GPT, Kirkcaldy, Scotland, manufacturing PC2286/PC2386 | Contemporary independent report: D pp. 9–10 | Show alongside B's Korean assembly statement. No invented factory chronology or batch assignment. |
| 8086/8 MHz, 640 KB parity RAM, 8087 socket | Documented: B p. 3 and back cover; M2086 p. 2 | Include. CPU brand is not inferred from all possible boards. |
| 80286/12 MHz, 1 MB parity RAM expandable to 4 MB on board, 80287 socket, LIM 4.0 | Documented: B p. 5 and back cover | Include. Do not copy the PC2386 cache figure into PC2286. |
| 80386/20 MHz, 4 MB parity RAM, 64 KB/35 ns cache, 80387 socket; 16 MB onboard limit | Documented: B pp. 6–7 and back cover; M2386 p. 2 (16 MB limit from B) | Include; no generic 386-board maximum substituted. |
| PC2086 S/D/30, 720 KB drives and 30 MB disk; PC2286 dual HD floppies or 40 MB disk; PC2386 HD floppy and 65 MB disk | Documented: B pp. 3, 5, 7; P; M2086/M2386 p. 2; C gives PC2386 1.44 MB | Include by model; the PC2286 hardware cell says HD rather than silently expanding the brochure's abbreviated capacity. |
| Integrated VGA, EGA/CGA/Hercules compatibility, bidirectional parallel, RS-232, external drives/tape connector | Documented: B p. 2 and back cover; M2086/M2386 p. 2 | Include as manufacturer specifications, not tested compatibility in BluMach. |
| PC2086 three 8-bit slots plus disk connection | Documented: B p. 3/back cover; M2086 p. 2 | Include. |
| PC2286 five 16-bit slots **versus** four 16-bit plus one 8-bit | Documented contradiction **within B**: p. 5 versus back-cover table | Include both in the hardware cell and explain both locators in technical. No chosen winner. |
| PC2386 five 16-bit slots **versus** four 16-bit plus one 8-bit | B p. 7 + M2386 p. 2 (also C) versus B back cover | Include both, name brochure and service manual. |
| PC2086 two 8 KiB system files, 32 KiB video file | Observed archive metadata: `pc2086-40179`, `pc2086-40180`, `pc2086-40186`; hashes rechecked | Include custody; physical chip ordering remains unresolved. No FPGA ROM bundle treated as an original. |
| PC2286 four 32 KiB chips; 64 KiB system/16 KiB video extracts; 2023/2026 byte agreement | Observed: F2286 and preserved acquisition reports | Include; matching filenames/content do not verify physical socket mapping. |
| PC2386 v1.11 attribution, four 32 KiB files and 64 KiB system reconstruction; old reset/video deficiencies | Observed: F2386 and existing PC2386 research records | Keep uploader attribution and old dump limitations. Permutations/patches are derivatives, not extra original revisions. |
| Archived PC2286/PC2386 chip files and system extraction match | Observed: R, five file pairs compared directly, hashes in private report | Include in both cards. No conclusion that boards are identical or that either ROM set boots either machine. |
| Dedicated PC2086 code; no dedicated PC2286/PC2386 machine registration in inspected base | Observed source: S, `machine_pc2086_init` and `.internal_name = "pc2086"` | Include under implementation only. Existing unmerged PC2386 experiments are not represented as mainline support. |

The user explicitly requested that conflicting documented values be displayed
with both sources, rather than omitted or silently resolved. No firm immediate
predecessor/successor relationship is added where not established by the sources
used here. Marketing wait-state/95-percent cache-hit claims are not promoted to
measured timing results.

## Preservation and unresolved work

Historical research cards remain `research`; none asserts successful POST,
normal usability or validated emulation. No firmware was executed or modified.
The prior PC2386 record's security/experimental observations and PC2086 FPGA
state are retained without being generalized to these new cards. PC2286's
earlier stale pending-acquisition wording remains historical context; the new
editorial supplement explicitly records custody.

Photos embedded in brochures are not licensed catalogue media merely because
they can be viewed. No new image is published: clear redistribution rights
remain pending. Third-party originals, firmware, screenshots of scanned pages
and the report collection stay outside Git. Only original editorial text and
metadata enter this PR.

Labbook candidates: the differing slot tables; factory attribution and dated
batch evidence; cross-model firmware identity, interleaving and reset vectors;
gate-array, cache and video mapping from manuals/datasheets/BIOS analysis.
These need separate treatment rather than expanding the catalogue cards.

## Validation scope

Run the catalogue builder, catalogue/source-link/family-history tests, release
identity check, existing-content contract and five-language web export. Audit
all three machine records and the shared library, then refresh only these three
preservation-index entries. The storage helper is absent in this checkout;
TEMP/TMP are explicitly directed to the task directory on G:.

No native build or visual review of these three cards is claimed. Firmware
execution, physical measurements and a component-level emulation implementation
audit are outside this editorial PR. Detailed results and the PR reference are
recorded in the local preservation worklog.
