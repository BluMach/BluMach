# PPC512 and PPC640 catalogue evidence

Editorial iteration, 6 November 2026. Spanish is the reference for English,
French, Italian and Portuguese. Both new entries are visible research cards,
without an executable creation profile. No emulator implementation, firmware
bytes or third-party images are added to the repository.

Base: `88f74ba30693e14b0003c656fa3ac578c3d85eed`. The family PR is independent
of Series 5 PR #236. All pre-existing products retain their visibility, status,
structure, translation keys and creation data. The existing research-card block
order is retained: commercial, architecture, firmware, emulation, research
material. Translation provenance notices are unchanged.

## Claims and sources

Identifiers refer to preservation manifests in the official local library.

| Claim | Preserved source and locator | Evidence and decision |
|---|---|---|
| Family advertised in Britain in February 1988 for office, home and travel; range from GBP 399 excluding VAT | `amstrad-ppc-advert-198802`, advertisement in *Personal Computer World*, dated by its archive page `amstrad-ppc-advert-page-20261106` | Original advertisement visually read. The price is the advertised starting price of the range, not a worldwide launch price or an exact PPC640 configuration. |
| PPC512 has 512 KB and lacks the modem and Mirror II; PPC640 has 640 KB, modem and Mirror II | `amstrad-ppc-service-manual`, printed p. 2 | Documented manufacturer specification, visually checked. Supports entry versus communications positioning; no invented OEM or successor. |
| One or two 720 KB 3.5-inch drives, MS-DOS 3.3; S/D suffixes | Service manual p. 2; `amstrad-ppc-museum-20261106`, specimen CH24547 | Primary specifications plus the museum's observed model nomenclature. Do not assume every PPC640 has two drives. |
| Processor clock 8 MHz; component CXQ70116P-8 at IC120 | Service manual pp. 2 and 30; `amstrad-ppc-technical-section1`, §1.1 | Parts list and technical description differ in naming: 8086/8086-2 versus CXQ70116P-8. Publish the documented part number and explain the distinction. No blanket claim about every PCB revision or physical inspection. |
| PPC512 RAM 00000h–7FFFFh, external 32 KB blocks up to 640 KB; PPC640 RAM 00000h–9FFFFh with parity | Technical manual §1.2 | Documented. No claim of implemented expansion or successful RAM diagnostics. |
| IDA CGA/MDA, LCD 640 × 200, CGA B8000h and MDA B0000h | Service manual p. 2; technical manual §1.11 | Documented. LCD is not called plasma; no PC1512-specific 16-colour graphics claim. |
| RS-232C, Centronics, optional 8087-2 and MC146818P | Service manual pp. 2, 30; technical manual §§1.1, 1.9–1.10 | Documented hardware only. |
| Adapter or ten C cells | Service manual p. 2 | Documented. A secondary account calls them D cells; prefer the manufacturer. Omit untested runtime promises. |
| PPC640 V.22bis 2400 bit/s and AT, COM2 2F8h–2FFh / IRQ3, national connector differences | Service manual p. 2; technical manual §1.16 | Documented. Do not infer universal telephone approval or emulated modem support. |
| PPC512 two 8 KiB system files and one 32 KiB character file | Existing `ppc512-rom-1`, `ppc512-rom-2`, `ppc512-rom-3` | Preserved bytes and hashes; emulator-collection attribution. File size is not proof of physical character-ROM capacity. |
| PPC640 six 8 KiB files, claimed 1.8/2.1; IC129 D3 anomaly | Existing `ppc640-rom-1` through `ppc640-rom-6`, `ppc640-static-comparison` | Observed static comparison; version labels are depositor claims. Suspected faulty read, no repaired bytes and no boot certification. |
| Inherited shared implementation, LCD/CGA/MDA video and ROM loader | `amstrad-ppc-blumach-machine-table-base`, `amstrad-ppc-blumach-machine-code-base`; `machine_ppc512_init`, profile `ppc512` | Read-only source inspection at the base commit. Separate from hardware validation. |

## Public source pages

- [Amstrad PPC512/640 Service Manual — Old Crap](https://oldcrap.org/2019/09/19/amstrad-ppc-512/), printed pp. 2 and 30.
- [Amstrad PPC Technical Manual — transcription by John Elliott](https://www.seasip.info/AmstradXT/ppctech/section1.html), §§1.1–1.2, 1.9–1.11 and 1.16.
- [Amstrad advertisement, Personal Computer World, February 1988 — nosher.net](https://nosher.net/archives/computers/amstrad_ppc512_percw_feb88).
- [PPC512D, specimen CH24547 — Centre for Computing History](https://www.computinghistory.org.uk/det/24547/Amstrad-PPC-512D/).
- [PPC640 deposit labelled 1.8 — Internet Archive](https://archive.org/details/amstrad-ppc-640-bios-upd-2764-d-dip-28-ic-132-toshiba-tmm-2464-ap).
- [PPC640 deposit labelled 2.1 — Internet Archive](https://archive.org/details/amstrad-ppc640-bios-dump-v2.1-character-rom).
- [BluMach Amstrad implementation at the inspected base](https://github.com/BluMach/BluMach/blob/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine/m_amstrad.c).

PPC512 firmware provenance remains in its canonical manifest, including the
pinned emulator-collection revision and individual hashes. No original dump
owner has been established. No firmware is published by this change.

## Emulation assessment and omissions

| Component | Assessment | Remaining work |
|---|---|---|
| Firmware | unknown/blocker | Physical attribution; PPC640 IC129 re-read; scan before execution |
| CPU/bus | compatible approximation | Exact processor instruction and timing behaviour |
| Memory/custom logic | compatible approximation | Parity, external expansion and reset behaviour |
| Interrupt/DMA/timers | compatible approximation | PPC-specific timing and reset validation |
| Keyboard | unknown/blocker | Controller and key protocol validation |
| RTC/NVRAM | compatible approximation | Reset and persistent-state checks |
| IDA/LCD | compatible approximation | Display modes, panel mapping and timing validation |
| Floppy | compatible approximation | Both drives and media diagnostics |
| Hard disk | unknown/blocker | External expansion/controller configurations; not claimed as standard |
| Serial/parallel | compatible approximation | Peripheral and interrupt checks |
| PPC640 modem | unknown/blocker | Hardware protocol and communications validation |

Conclusion: viable after named research and validation work; historical cards
are publishable independently of emulation readiness. No exact first-sale day,
single worldwide launch date, named OEM or direct successor is asserted.
No new media field is added: suitable photography with clear redistribution
rights has not been established in the inspected records. Newly preserved
scans and third-party snapshots are local-only. Migration remains in progress.

Labbook candidates: PPC CPU naming and board revisions; character-ROM banks
and physical capacity; PPC640 D3 anomaly; IDA LCD scan conversion and timing.

The common text `collection.unpreserved` previously said firmware was not
preserved whenever a product lacked a platform. Its existing five translations
now say that no emulation profile exists, matching the actual UI condition.
This is a text correction; no schema, filter, platform or emulator code changes.

## Validation

Run both machine audits, catalogue builder and catalogue tests, source-link
tests, family-history tests and release-identity validation. The local contract
check verifies existing products remain identical and both new cards appear in
all five website languages without creation profiles. Results are recorded in
the PR and canonical worklog. No firmware execution or native GUI review is
claimed for these PPC cards.
