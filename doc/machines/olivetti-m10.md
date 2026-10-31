# Olivetti M10

Status: experimental. The M10 runs on the CoreMach engine, not on the 86Box
core, and needs a BluMach build with CoreMach. For the evidence-to-model
account see the [M10 engineering notes](olivetti-m10-implementation.md).

## Historical identity

Olivetti introduced the M10 in 1983, a battery-powered 8-bit portable with an
OKI 80C85, 8 to 32 KB of battery-backed RAM, 32 KB of ROM with resident
programs and Microsoft BASIC, and a tilting 240 x 64 LCD. It was built by
Kyocera and is related to the Kyocera Kyotronic 85, Tandy TRS-80 Model 100
and NEC PC-8201, but none of them is an alias of the M10: ROM, keyboard,
case and board differ. The US M10 MODEM variant, with a 300 bit/s Bell 103
modem, needs a different ROM that has not been preserved and is not offered.

## Firmware

BluMach expects the first 32 KB of the system ROM at
`roms/machines/olivetti_m10/m10-system-first32k.bin`, SHA-256
`43f85e3ac5cc264be3ad4c3b36dea82381409a66381146ce06a78c22704b0dbc` (the
image MAME declares as `m10rom.m12`). It is not distributed with BluMach.

## Creating and using the machine

The catalogue offers 8, 16, 24 or 32 KB of RAM, the keyboard language jumpers
(Italian, French, German, British) and the clock start: host time, running on
while the machine is closed, or a fixed reproducible start. RAM and clock are
kept in `state/` in the machine folder.

In the machine window, Action holds pause, reset, Switch on (after the
automatic power-off), sound, the serial port, the cassette and the printer;
View holds the scale and the LCD look. Settings is not available for CoreMach
machines: create a new machine to change its RAM or jumpers.

## Not emulated

Bar-code reader, the US modem, the low-battery TRAP and the 81C55's 256 bytes
of RAM; RTS, DTR, CTS and BREAK on the serial line.
