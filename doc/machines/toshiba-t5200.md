# Toshiba T5200

The Toshiba T5200 is a mains-powered transportable PC with a 20 MHz Intel
80386DX, external cache, VGA graphics and an integrated 11.5-inch orange
gas-plasma display. This entry covers the plasma T5200, T5200/100 and T5200/200
architecture. It does not represent the colour-LCD T5200C.

Its BluMach status is **experimental**. The original firmware boots Toshiba
MS-DOS 3.30 and the supported configurations below are usable, but several
Toshiba gate arrays are represented by narrow behavioural models. See the
[implementation narrative](toshiba-t5200-implementation.md) for the evidence,
failed hypotheses and replacement criteria behind those compromises.

The optional 3inONEder support is an independent interoperability
implementation. BluMach and its contributors are not affiliated with or
endorsed by Conventional Memories. No manufacturer source code, firmware,
artwork, logos or other product assets are included.

## Supported configuration

- Intel 80386DX at 20 MHz, with optional 80387 through the normal machine UI.
- 2, 4, 6, 8, 10, 12 or 14 MB RAM; every selectable size has passed Toshiba
  TEST3 and the Award V1.30 POST memory path.
- One internal 3.5-inch 1.44 MB or 720 KB floppy drive.
- Toshiba OEM Paradise PVGA1 with 256 KB VRAM.
- Simultaneous external colour VGA and internal orange plasma output by
  default, or external VGA only.
- IDE initialization is present, but the documented Conner disks are omitted
  from catalogue creation until their geometries and firmware behavior are
  validated.
- The half-length expansion position can be configured as its documented
  Toshiba-only **A form factor** instead of ISA-8. With the documented
  Conventional Memories 3inONEder selected, its YMF262-M/YAC512-M OPL3 sound
  section is available at 388h, 220h, 240h or dual 388h/220h decoding. Its CF
  bridge uses user-supplied firmware at C8000h and I/O 300h (AT-INT) or 320h
  (AT320INT); each image remains a separate local input.
- Dual 388h/220h decoding is the publicly documented default. Disabling
  OPL3 while enabling an XTIDE image represents the CF-only version A.
- The documented PC joystick port is available at 201h, using the IBM Game
  Control Adapter-compatible four-axis/four-button behavior described in the
  public product documentation. Ethernet is optional (version C): it is modeled as the stated
  8-bit, partially NE2000-compatible controller at its documented factory
  default of 300h/IRQ5, without a network boot ROM. Selecting it with XTIDE
  requires the AT320INT image at 320h because 300h belongs to Ethernet.

The firmware's Plasma/CRT-only selection controls whether the internal panel
is active. When CRT-only has blanked the panel, **Ctrl+Home** performs the
documented runtime recovery while leaving external VGA active. The reverse
shortcut and physical CRT indicator are not known well enough to emulate.

## Required firmware

Place these files below `roms/machines/t5200/`. They are not distributed with
BluMach.

| File | Size | SHA-256 |
|---|---:|---|
| `t5200-award-v130.bin` | 131,072 bytes | `b64f034416eeb4eff277a126e17cea63b65b969e7c167d185c2af767adc31c68` |
| `t5200-vga-1988.bin` | 32,768 bytes | `baeee31a5cbc4c3f8505c93e483ae87a0eb529a900f41d5fb1c0e2545f5ccf14` |

The VGA device maps the declared 24 KB image through its 32 KB EPROM dump.

To enable the 3inONEder CompactFlash/XTIDE setting, obtain the compatible
firmware separately and copy it into the local ROM directory. BluMach does not
provide or distribute it:

| Local file | Required variant | I/O base |
|---|---|---:|
| `roms/machines/t5200/3inoneder/3inoneder-at-int.bin` | AT-INT | 300h |
| `roms/machines/t5200/3inoneder/3inoneder-at320int.bin` | AT320INT | 320h |

Use the normal hard-disk dialog to create or attach the CompactFlash image as
an IDE disk on channel **1:0** (or **1:1** for a second device). Channels
0:0/0:1 remain the T5200's internal IDE controller; the conventional IDE
dialog is intentionally reused rather than presenting CF as a floppy.

## Validation

The implementation has passed scoped checks for:

- clean POST and Toshiba MS-DOS 3.30 boot with Award V1.30;
- all seven selectable RAM populations and a direct EMS selector/page probe;
- 720 KB and 1.44 MB floppy boot/read paths, plus 720 KB write persistence and
  write protection;
- Toshiba TEST3 identification, memory tests and saved video choices;
- simultaneous VGA and plasma output in VGA and CGA modes;
- VGA colour/monochrome, plasma brightness, CGA two/16-level and Single/Double
  presentation choices;
- VCHAD access to the 64-byte PDC table and its sixteen intensity ordinals;
- runtime Ctrl+Home restoration from CRT-only without changing the external
  output or saved CMOS selection; and
- catalogue creation and configuration with Qt 6.

These checks establish a functional vertical slice, not cycle accuracy.

## Known limitations

- The T4758A and T9761 integration, cache path and platform latches use standard
  AT devices plus firmware-visible behavioural subsets.
- The sixteen EMS selectors and four D0000h slots work, but their physical
  gate-array owner, invalid-page electrical behavior and timing are unknown.
- The plasma conversion implements observed PDC tables and firmware choices;
  its orange transfer curve is visually inferred and not calibrated from real
  hardware.
- Exact regional characters require the missing 64 KB CG-ROM.
- The A-form endpoint is specific to the T5200 and models that later receive
  positive connector evidence. It is mutually exclusive with the half-length
  ISA-8 position; PJ12's 16-bit extension, electrical timing/DMA behavior and
  arbitrary ISA-card compatibility remain unmodeled.
- The 3inONEder Ethernet model uses a shared 8-bit NE2000-compatible core at
  the publicly documented factory resources. Its exact controller, RSET8019
  persistent configuration mechanism, MAC/NVRAM behavior, analogue network
  front end and packet-driver interoperability remain to be validated.
- External-floppy routing and documented Conner hard disks remain incomplete.
- Ctrl+Home currently reaches the PDC model directly. The original 8749
  SCC/8042/BIOS notification transaction and CRT indicator are not reproduced.

## Sources

- [Toshiba T5200 Maintenance Manual](https://archive.org/details/toshiba-t-5200-maintenance-manual)
- [Toshiba T5200 and T5200C summary](https://www.minuszerodegrees.net/manuals/Toshiba/Other/Toshiba%20T5200%20and%20T5200C%20-%20Summary.pdf)
- [T5200 TECHaccess specification](https://conventionalmemories.com/Toshiba/TECHaccess/tech5580.htm)
- [3inONEder developer reference](https://conventionalmemories.com/wiki_cm/3inONEder_for_Toshiba_portables)

The canonical local preservation record is `library/toshiba/t5200`, stable ID
`t5200`. Firmware, diagnostics, disks, manuals and validation captures remain
outside the public BluMach repository.
