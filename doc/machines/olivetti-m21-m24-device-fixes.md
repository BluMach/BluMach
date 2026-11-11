<!-- BluMach modifications: rtzor, Project BluMach, 2026. -->
# Olivetti M21/M24 device corrections

BluMach now decodes the documented OGC CRTC aliases, models the MM58174
STOP/START latch and reloads the FAST PIT in mode 4 while GATE is low.
These shared-device corrections were investigated with original Olivetti
firmware and Customer Test 3.1. The M24 SP remains a research entry while its
board timing, firmware donor and slow-mode circuit require further evidence.

## Evidence and behavior

The M21/M24 *Theory of Operation*, printed pages 5-11, 5-13 and 5-29/30,
documents the CRTC address pair with A1/A2 ignored. The diagnostic accesses
3D0h/3D1h; the wrapper previously accepted only 3D4h/3D5h, so it returned FFh.
The GO380 profile now maps 3D0h–3D7h to the canonical pair while preserving A0.
Ports 3D8h–3DFh keep their existing behavior. Other OGC profiles retain their
previous decode until their own hardware evidence establishes those aliases.
See the [Olivetti M21/M24 manual index](https://ftpmirror.your.org/pub/misc/bitsavers/pdf/olivetti/m21_m24/).

National Semiconductor's *MOS Databook 1980*, MM58174 section, printed pages
5-355–5-357, describes a STOP latch that holds the prescaler, tenths and seconds
counters in reset. Writing zero stops the clock; writing one releases reset.
A repeated START preserves the running phase. BluMach previously cleared the
seconds on START but retained the NVR divider phase, and host synchronization
suppressed even that reset. The device now applies STOP/START independently of
the initial synchronization source, exposes tenths and ignores writes to the
read-only seconds counters. Its per-instance 10 ms NVR timer supplies a new
phase on STOP-to-START. The later MM58174A sheet was used as a comparison, with
the original part's contract established from the 1980 publication.
See the [National data-book index](https://ftpmirror.your.org/pub/misc/bitsavers/components/national/_dataBooks/).

The native diagnostic also programs channel 2 as a binary mode-4 strobe and
compares consecutive counts through port 42h. With GATE low, FAST PIT left a
new count pending indefinitely and exposed the old count. Mode 4 requires the
count register to transfer into the counting element on the next CLK even
while GATE inhibits decrementing. A low strobe lasts one CLK. The shared
semantics are described in the *Intel 8254*, 231164-005, mode-4 section,
available through the [CMU course archive](https://www.cs.cmu.edu/~410/).
This is a behavioral reference; Olivetti documents an 8253 in the M24.

The PIT implementation keeps a phase reference per internally clocked channel.
It preserves that phase across timer rebases and period changes, maintains
pending loads and output recovery across internal/external clock transitions,
and unlinks all its timers on close. Scheduler dispatch still has its existing
whole-TSC resolution. The original timer comparison and the diagnostic remain
unchanged.

## Existing NVR files

The MM58174 has 16 software-visible registers. Existing BluMach files contain
exactly those 16 bytes and were produced by a model that always ran, regardless
of the stored STOP bit. On first load they migrate to RUN while retaining the
stored date and time when host synchronization is disabled. A fresh emulated
clock also starts running.

New saves append a private 16-byte footer after the unchanged register layout:
ASCII `BMM58174`, version byte `01h`, then seven reserved zero bytes. The footer
is outside the guest I/O window. It identifies the format in which an explicit
STOP must remain stopped after reopening. Its seconds are normalized to zero
before the internal clock is initialized. With host synchronization enabled,
the initial date/time continues to come from the host.

Unknown, future or incomplete files are preserved on disk. The usable register
prefix can initialize the device, while persistence stays disabled and one
operational warning identifies that condition. Each load restores the expected
16-byte prefix size before reading, including repeated loads on the same
instance. The generic NVR implementation and other RTC formats are unchanged.

Older builds can read the register prefix but discard the footer when saving;
they also retain their original always-running behavior.

## Validation

Run the native contracts with GCC available:

```text
python -B -m unittest tools.tests.test_olivetti_ogc_io tools.tests.test_olivetti_m24_rtc tools.tests.test_pit_fast_mode4 -v
```

The tests compile the production C translation units. They cover:

- OGC alias pairs, all cursor-low byte values, register masks, extended ports,
  control addresses, unaffected profiles and text/graphics timing paths.
- MM58174 STOP/START, repeated START, prior divider phases and read-only
  counters, plus real NVR loading/saving of legacy, current and unknown files.
- PIT loads, gating, retriggering, one-clock strobes, fractional periods,
  clock changes, TSC rebases, source transitions and timer ownership. The real
  scheduler is included. Existing sequences of other modes are also checked.

Original ROMs and diagnostic disks are local preservation inputs. Runtime
results and exact binary/configuration hashes belong to their local machine
records; they are separate from these firmware-free tests. No ROM, operating
system, diagnostic binary or restricted manual is included in this change.

## Remaining boundaries

The RTC uses a 10 ms scheduling approximation rather than the physical crystal
divider. Data-changed, test mode, interrupt behavior and the pre-existing
calendar-rollover issue require separate work. The control-register readback
is retained compatibility behavior rather than a documented physical output.
The optional dual-RTC architecture also remains outside this correction.

FAST PIT keeps its inherited BCD limitations. Device contract coverage does not
establish complete validation of every AT, PAS16 or 3270 consumer, audio path or
IRQ interaction. Those checks remain distinct from a successful POST or DOS
boot. The SP's effective slow speed and its physical CLK/READY mechanism are
also separate research questions; the present change adds no speed selector.
