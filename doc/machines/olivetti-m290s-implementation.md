# Olivetti M290S engineering notes

BluMach provides a **partial, experimental M290S** profile for the M203 board
and BIOS 1.29. It can create 1, 3 or 5 MiB configurations, with 1 MiB as the
default, and compatible blank IDE disks using the commercial 20/40/100 MB
geometries. These results do not establish an exact reconstruction of the
TACT/OLIMCU chipset or its integrated hard-disk circuitry.

## Identity and evidence

The [M203 board record](https://theretroweb.com/motherboards/s/olivetti-m203)
associates the board with M290S and preserves the BIOS and original tutorial.
The [Olivetti brochure preserved by VintAds](https://www.vintads.it/file.php?cod=158)
documents the 5 MB motherboard limit and advertises 17 MB for the system.
The latter is not validated usable RAM in BluMach. The catalogue retains the
conflicting 12 MHz museum description alongside the documented 16 MHz offer;
it does not create a separate 12 MHz variant from that discrepancy.

The local BIOS set contains two 64 KiB chips. LOW supplies even bytes and HIGH
odd bytes in the 128 KiB map at E0000h–FFFFFh. Version 1.29 is present at combined
offset 1E02Bh. The literal date string is not treated as a proven release date.
Hashes and provenance are preserved privately; firmware and original media
are not distributed with these notes.

## Component choices and compromises

| Component | BluMach representation | Evidence boundary |
| --- | --- | --- |
| CPU | 80286, 16 MHz | Commercial evidence; the 12 MHz discrepancy remains open. |
| RAM | 1 MiB bank plus up to two 2 MiB banks | Represents the soldered MiB and optional SIMM pairs; physical population/parity rules remain unverified. |
| Memory control | M203-specific TACT/OLIMCU behaviour | Derived from BIOS-visible register sequences and runtime observations, not recovered schematics. |
| Video | Existing Paradise PVGA1A model | Matches the documented onboard controller; not a full board-level validation. |
| Peripheral logic | Reused PC87310 and Olivetti keyboard-controller behaviour | Compatible implementation; exact physical control lines remain unidentified. |
| HDD | Compatible PC/AT IDE task file | BIOS, tutorial and the 40-position header support the trial; exact pinout and host circuitry are unconfirmed. |

The dedicated `olivetti_m290s` profile separates this firmware from PCS 286/S.
With no disk, creation leaves ATA absent: an empty responding controller was
enough to trigger Extended Setup. With a disk, the compatible controller uses
the BIOS geometries 615/4/17, 981/5/17 or 755/16/17. It is not presented as a
positively identified original ISA card or host chip.

## Memory allocation and shadow

The firmware exposes indexed control through FFFEAh, FFFECh and FFFEEh. An
AAh/55h unlock sequence permits register access; ordinary bytes in that ROM
page retain the selected BIOS or shadow backing. BIOS-observed register 0Fh
limits extended allocation in 512 KiB units. Register 0Eh selects and protects
the E0000h/F0000h shadow blocks. RAM reserved there must no longer appear in
the reclaimed memory above installed DRAM.

Saving reservations originally produced wrong POST totals or a shadow error.
The corrected bank sizing, allocation and protection let the original Setup
save them and DOS boot in a new emulator process. At 5 MiB the measured
extended totals are 4416 KiB with 64 KiB shadow, 4352 KiB with 128 KiB shadow
or 128 KiB EMS, and 4224 KiB with both 128 KiB reservations. This validates
allocation and persistence, not the original LIM/EMS manager API.

The shared mapping recalculation also needed to start at the greater of the
requested address and mapping base. Starting earlier could replace preceding
sibling windows. Its production-code contract covers CPU/DMA/executable
views, neighbouring 4 KiB pages, the final 4 GiB page and masked aliases.

M203 direct execution backing now follows the same physical translation as
data. Up to three linear segments represent high DRAM and reclaimed RAM,
including the split caused by an E0000h-only reservation. Page-frame windows
use their selected backing; absent banks return FFh and ignore writes. An
enabled A5h page write updates an established global frame while retaining
the separate bank-discovery transaction.

An initial review suspected that missing direct backing prevented protected
execution on the supported 286. Native control tests disproved that claim:
this interpreter fetches through callbacks and already passed. The added
backing aligns the generic cache contract; it is not proof of an earlier
286 protected-mode failure. Both control and corrected results are retained.

## Validation and remaining work

The combined implementation cold-boots Olivetti DOS 5.00 from floppy at
1/3/5 MiB and from the compatible 100 MB IDE disk at 5 MiB. The earlier
20/40/100 MB IDE tests were at 1 MiB; all nine combinations are not claimed.
Original Setup CMOS configuration persists with error register 0Eh zero.
Older CMOS profiles may need one new save from the original Setup after bank
sizing changes; no user state is silently rewritten.

A project-authored 286 probe executes protected-mode code at low and top XMS
addresses before and after the backing correction. Corrected runs also pass
with 64 KiB shadow and with 128 KiB shadow plus 128 KiB EMS. Host contracts
exercise the actual mapper, register handlers and direct cache access without
firmware, with assertions enabled in Debug and Release. The shared change
also passes the original M24 MAIN BOARD diagnostic with BIOS 1.43 and 1.44.

Still open are matched original LIM/EMS software, the complete physical
TACT/OLIMCU map, SIMM/parity rules, other M203 revisions, advertised system
expansion, exact HDD circuitry and wider peripheral/timing validation. The
catalogue therefore remains partial and keeps its attribution warning.
