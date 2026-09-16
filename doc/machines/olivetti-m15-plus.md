# Olivetti M15 Plus

Status as of 2026-09-13: experimental but bootable pilot. BluMach passes every
displayed Resident Diagnostics item, reports the documented 512 KB and boots
the original 720 KB System Test disk into MS-DOS 3.20. The original customer
test also exercises both documented graphics modes and the LCD shade table.
The alternative 20 MB HDU configuration now has an experimental host-interface
model and still requires validation with the original HDU diagnostics.

For the evidence-to-model account and replacement criteria, see the
[M15 Plus engineering notes](olivetti-m15-plus-implementation.md).

## Historical identity

Olivetti introduced the M15 Plus in 1988 as a distinct successor in the M15
portable family. It combines an Intel 80C88 at 4.77 MHz, 512 KB of user RAM,
16 KB of video RAM, a detachable 78-key keyboard and a 10.5-inch backlit
super-twist monochrome LCD.

Olivetti documented two commercial storage configurations: two internal
3.5-inch 720 KB floppy drives, or one such drive plus a 20 MB internal HDU.
Both could use an optional external 5.25-inch 360 KB drive. The catalogue
offers both internal configurations; the external unit is not represented.

Olivetti calls the internal hard-disk link SCSI. The service guide names Epson
HMD755 and Fujitsu FK308S-39R mechanisms, while BIOS 1.10 exposes a six-byte
command interface at `0320h-0323h`, IRQ 5 and DMA 3. BluMach therefore gives
the M15 Plus a dedicated HDU/SCSI device while reusing the compatible XTA host
state machine for that observed register contract. This does not claim that
the unrecovered physical bridge was itself a generic XTA controller.

## Display

The operations guide documents 40×25 and 80×25 text plus 320×200 and 640×200
graphics. A contemporary Olivetti brochure instead advertises a maximum
resolution of 640×320. That may describe the physical panel rather than a BIOS
mode, but the conflict is unresolved and BluMach does not advertise 640×320.

BIOS 1.10 programs the CGA-compatible and V6355D extended ports already used
by the M15 family. The pilot therefore uses the fixed green monochrome V6355D
LCD presentation, with the original BIOS glyphs `00h`–`7Fh` read from their
mapped address. The original M15 Plus test writes the RGBI sequence
`7, 3, 1, F, B, 9, 8, 0` for black, six greys and white. BluMach preserves
that observed eight-level, positive-LCD ordering; the analogue response of the
real panel remains approximate. No external video connector is documented or
exposed.

## Recommended BluMach configuration

- machine: `[8088] Olivetti M15 Plus (experimental)` (`olivetti_m15plus`);
- CPU: Intel 80C88-compatible core at 4.77 MHz;
- memory: fixed 512 KB;
- video: fixed internal V6355D-compatible green LCD;
- storage: either two internal 3.5-inch 720 KB drives, or one such drive plus
  the experimental 20 MB HDU/SCSI device;
- hard disk geometry: 615 cylinders, 4 heads and 17 sectors per track.

The historical catalogue creates either sales configuration in the common
declarative modal. The HDU choice generates a blank 20 MB image and records the
host-state-machine approximation visibly.

## Firmware

The locally researched image identifies as system BIOS 1.10 and carries the
date string `06/03/88`. BluMach expects its 32 KB image as:

`roms/machines/olivetti_m15plus/Olivetti-M15-Plus-BIOS-1.10.bin`

It maps at `F8000h-FFFFFh`; the reset vector reaches `F000:8050h`. Firmware
provenance and redistribution rights have not been verified, so no BIOS or
proprietary software image is included with BluMach.

## Firmware-backed configuration

Memory is fixed at the documented 512 KB. The M15 Plus BIOS retains the M15
family's multiplexed board-switch reads and decodes the same 40- and 80-column
startup values. Configure therefore exposes only that startup text choice,
defaulting to 80 columns. It is a boot preference, not a restriction on later
software-selected display modes.

## Known approximations and pending work

- The available NMOS 8088 core and calibrated PIT ratio approximate an 80C88.
- The eight observed LCD levels are reproduced, but their brightness curve and
  the other eight RGBI-code reductions do not model the physical super-twist
  panel, backlight, contrast circuit or response time.
- The keyboard-switch block, MSM6242 RTC register behavior, UART, LPT and FDC
  are compatible models supported by BIOS analysis, not recovered board logic.
- The HDU host register block is implemented at `0320h-0323h`, IRQ 5 and DMA 3.
  The unidentified physical SCSI bridge and drive firmware remain approximate;
  raw disk images cannot retain controller ECC bytes, so READ LONG supplies a
  stable four-byte diagnostic field and WRITE LONG discards it.
- Characters `80h`–`FFh` retain the generic high-character fallback.
- Cold POST, soft and hard reset, 40/80-column startup, the original System
  Test boot, the LCD character/shade screens and 320×200/640×200 graphics are
  validated. The customer test's Memory module passes. Its System Board and
  Display LCD modules still return `FAILED`, and its configuration report sees
  one 360 KB drive rather than the configured pair of 720 KB drives; these are
  explicit evidence of missing board/equipment-flag behavior. The special
  78-key `EDIT/SHIFT` path also remains pending.

## Principal references

- *Olivetti M15 Plus Service Guide*, system-board and mass-storage sections:
  <https://www.ardent-tool.com/Olivetti/Docs/service_guide/systems2/capk.pdf>
- *Olivetti M15 Plus Installation and Operations Guide*, first edition,
  March 1988: <https://mail.minuszerodegrees.net/manuals/Olivetti/Olivetti%20-%20M15%20Plus%20-%20Installation%20and%20Operations%20Guide.pdf>
- *Olivetti Personal Computer M15 Plus* brochure, 01200090 G, March 1988:
  <https://www.museotecnologicamente.it/wp-content/uploads/M15_Depliant_inglese.pdf>
- BIOS index and metadata:
  <https://minuszerodegrees.net/rom/rom.htm>
- M15 Plus software catalogue:
  <https://olivrea.de/software/>
