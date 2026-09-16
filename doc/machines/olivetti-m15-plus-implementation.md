# Olivetti M15 Plus engineering notes

## Outcome and evidence boundary

BluMach's M15 Plus implementation is an **experimental dual-configuration
pilot**. It gives BIOS 1.10 a distinct 32 KB ROM map and reuses only those M15
family interfaces for which the Plus firmware shows the same guest-visible
contract. The catalogue can create either the dual-floppy system or the
commercial one-floppy/20 MB HDU configuration.

Here, *documented* means stated by contemporary Olivetti material, *observed*
means visible in static firmware analysis or a reproducible emulator test,
*inferred* means a design conclusion supported by several observations, and
*approximate* identifies a compatible substitute for unrecovered circuitry.
Reaching POST will not by itself prove the original motherboard has been
reconstructed.

## Why the machine is difficult

The operations guide describes user-facing hardware but provides no schematic,
service-level I/O map or controller identities. The surviving 32 KB BIOS has
unverified physical provenance and cannot be distributed. The Plus also has a
proprietary 20 MB HDU path, a backlit panel that differs physically from the
M15 display, and a brochure claim of 640×320 that conflicts with the guide's
320×200 and 640×200 BIOS graphics modes.

Treating the Plus as an M15 alias would hide those differences. Treating it as
a generic XT would lose the firmware's unusual memory-switch, RTC and video
contracts. The pilot therefore has a separate machine identity while sharing
small, evidence-backed behavioral components.

## From firmware to the component map

The 32 KB image ends with a far jump to `F000:8050h`, so it maps at
`F8000h-FFFFFh`. Its 128-glyph, 8×8 ASCII table begins at file offset `7A6Eh`,
which resolves to physical `FFA6Eh`: exactly the address used by the M15 BIOS
font loader. This permits direct font reads from mapped firmware with no copied
or generated font asset.

Disassembly shows immediate access to CGA/V6355D-compatible ports `03D4h`,
`03D8h`, `03D9h`, `03DDh` and `03DFh`, including the extended register path.
It also repeats the M15 sequence that toggles port `61h`, reads multiplexed
configuration nibbles at `62h`, and obtains startup display bits through
`60h`. Those observations justify the shared board-switch behavior and the
configurable 40/80-column startup selector.

The clock code asserts HOLD at `010Dh`, transfers BCD digits through
`0100h-010Ch`, and accesses the remaining control registers through `010Fh`.
That is the same guest-visible MSM6242 contract already implemented for M15.
The floppy path is provisionally supplied by the compatible XT FDC used by the
working M15 model.

The HDU routines poll and command ports `0320h-0323h`, use IRQ 5 and DMA 3, and
issue six-byte Xebec/SASI-style commands. The BIOS parameter table describes
615 cylinders, 4 heads and 17 sectors per track, exactly 21,411,840 bytes. The
official service guide calls the interface SCSI and names Epson HMD755 and
Fujitsu FK308S-39R mechanisms. The surviving HMD755 data describes an IBM XT
bus variant, while the Fujitsu unit is a SCSI mechanism. This mixed evidence is
consistent with an Olivetti host bridge whose physical implementation has not
yet been identified.

The emulator consequently registers a dedicated M15 Plus HDU/SCSI device, not
a selectable generic card. Internally it reuses the mature XTA state machine
because that matches the BIOS-visible ports, handshaking, IRQ, DMA and command
packet. Missing sector-buffer and READ/WRITE LONG commands were added. A raw
image stores only 512-byte sectors, so READ LONG synthesizes four stable zero
ECC bytes and WRITE LONG ignores the received ECC field. Those choices are
explicit diagnostic approximations, not claims about the physical bridge.

## Compromise ledger

| Subsystem | Pilot treatment | Replacement evidence |
|---|---|---|
| 80C88 and timing | Fixed 4.77 MHz 8088 core plus the calibrated M15-family PIT ratio. | Physical timer trace or a dedicated CMOS 80C88 timing model. |
| RAM and switches | Fixed documented 512 KB; firmware-observed multiplexed switch encoding. | Board schematic or switch table confirming electrical wiring. |
| BIOS and font | Exact local 32 KB mapping; ASCII glyphs read at `FFA6Eh`. No firmware is distributed. | A provenance-confirmed, redistributable dump would change packaging, not the map. |
| V6355D and LCD | Existing register-compatible controller with fixed green eight-level presentation. The level count follows the original System Test; the luminance curve remains approximate. | Panel/controller identification and traces for backlight, contrast, timings and the 640×320 claim. |
| RTC | Existing MSM6242 BCD/HOLD model at `0100h-010Fh`. | Chip identification or board trace if Plus control side effects differ. |
| Keyboard and board probe | Shared M15-family port and switch subset with separate Plus device identity. | Keyboard firmware, protocol capture or schematic. |
| Floppy, UART and LPT | Compatible XT-era devices on firmware-visible routes. | Controller identification and timing traces. |
| 20 MB HDU | Dedicated integrated device at `0320h-0323h`, IRQ 5 and DMA 3; XTA host state machine; fixed 615/4/17 image; synthetic long-sector ECC. | Controller/bridge identification, board trace, drive firmware and original HDU diagnostic results. |

## Validation ladder

The static gate verifies the ROM size and reset address, font location,
switch-reading sequences, video ports, RTC range and HDU port boundary. A clean
Qt 6 build then cold-booted BIOS 1.10, passed every displayed Resident
Diagnostics item, reported `RAM 512/512 Pass`, loaded the original 720 KB
System Test disk into MS-DOS 3.20 and opened `M15PLUS SYSTEM TEST` version 1.00.
This validates the vertical slice through firmware, LCD text and floppy I/O;
it does not yet validate every test-menu subsystem.

The original Keyboard Drivers & Utilities disk also boots and reaches its
five-language selector. Its supplied LCD test identifies eight intended
monochrome levels (`BLACK`, `GREY1` through `GREY6`, and `WHITE`) and names
320×200 four-colour and 640×200 mid-resolution graphics tests. This is direct
software evidence for the eight-level LCD conversion. A runtime trace of that
screen showed the exact BLACK-to-WHITE RGBI code sequence
`7, 3, 1, F, B, 9, 8, 0`; the Plus presentation maps those observed codes
directly. The response of the physical panel and the reduction of the eight
remaining RGBI codes are still approximate.

Runtime validation now covers soft and hard reset, 40- and 80-column startup,
the customer test's Memory, System Board and Display LCD modules, the complete
LCD character and shade sequence, 320×200 four-colour graphics and 640×200
mid-resolution graphics. All three modules return `PASSED`, and the
configuration report identifies both internal 720 KB drives after the FDC
output-register, CRTC-alias and autonomous RTC corrections. The dedicated
78-key `EDIT/SHIFT` latch, editing keys, alternate F1-F10 codes and integrated
numeric keypad are implemented from the manual and `KBD.CUS` disassembly, but
their final original-utility run remains pending.

The HDU path is a new validation rung: compilation and catalogue creation prove
the configuration can be assembled, while the original HDU test must still
establish command timing, DMA completion and error reporting. No original media
is mounted writable, and proprietary firmware, manuals and disks remain outside
Git.

## Rejected shortcuts and replacement criteria

The implementation rejects an M15 alias because it would erase the Plus ROM,
display and storage identity. It rejects a C000h video option ROM because the
system BIOS directly owns the video path. It rejects a generic 20 MB XT-IDE or
MFM template because firmware accesses a different register block. Finally, it
does not turn the brochure's 640×320 wording into a mode without a trace that
explains how software selects it.

A generic XT-IDE or MFM card remains rejected because either would expose a
different host contract. Reuse of the XTA state machine is narrower: it sits
behind a machine-specific integrated device and implements only the contract
observed in the M15 Plus BIOS.

A later revision should replace an approximation only when a schematic,
readable board photograph, physical trace, controller dump or repeatable
machine/software test establishes a stronger contract. Until then the narrow
model and its declared SCSI/XTA boundary are easier to audit and less likely to
fossilize a convenient but false design.

## Implementation map

- `src/machine/m_xt.c`: 32 KB firmware mapping and shared M15-family setup;
- `src/machine/machine_table.c`: fixed CPU, RAM, video and floppy identity;
- `src/disk/hdc_xta.c`: machine-specific integrated HDU/SCSI device and the
  BIOS-observed host commands, including buffer and long-sector transfers;
- `src/device/kbc_xt.c`: separate Plus device identity plus the documented
  latching `EDIT/SHIFT` transformations and diagnostic-observed F-key bank;
- `src/video/vid_cga_v6355.c`: fixed green V6355D-compatible LCD rendering;
- `src/qt/catalog/source/machines/olivetti/olivetti-m15-plus/`: multilingual
  sheet and declarative sales-configuration selector for both documented
  internal storage configurations.
