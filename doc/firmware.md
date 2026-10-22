# Firmware policy and setup

BluMach emulates hardware but does not distribute proprietary system BIOS,
option ROM, controller firmware, operating-system media or application
software. A catalogue entry or preservation record may identify a required
file by name, size and hash; that metadata does not grant permission to copy or
redistribute the file.

## Providing local firmware

The Qt collection can be explored without firmware. Select the root ROM folder
in Tools → Preferences, then confirm to save it. Open folder and Check again
help verify the layout. Explicit `--rompath` has priority at startup.
Missing firmware prevents emulation but does not replace the saved machine or
video card. See the [first-alpha setup guide](releases/0.1.0-alpha.1.md) for the
three initial PCS firmware layouts.
The [second-alpha guide](releases/0.1.0-alpha.2.md) describes the additional
portable candidates and their firmware limitations.

Use firmware obtained from hardware you own or from another source whose terms
allow you to use it. BluMach searches a directory named `roms` beside the
application and BluMach's platform-specific data directories. On Windows the
shared per-user location is `%LOCALAPPDATA%/BluMach/roms`. The folder selected
in Preferences is used by machines started from the manager. To use an
explicit location, start BluMach with:

```text
BluMach --rompath path
```

`-R path` is the equivalent short form. BluMach prints the searched ROM paths
to its log during startup. Required relative filenames and layouts are listed
in the relevant public machine note when they can be disclosed safely.

## CoreMach machines

Machines run by the [CoreMach](https://github.com/Palimbit/CoreMach) engine
use the same ROM folders. Their firmware must match the expected size and
SHA-256 exactly; another file at the same path is refused and the saved
configuration is left unchanged.

| Machine | Relative path | Size | SHA-256 |
| --- | --- | --- | --- |
| Olivetti M10 | `machines/olivetti_m10/m10-system-first32k.bin` | 32 768 bytes | `43f85e3ac5cc264be3ad4c3b36dea82381409a66381146ce06a78c22704b0dbc` |

The battery-backed RAM and clock of a CoreMach machine are kept in its machine
folder as `state/ram.bin` and `state/rtc.bin`.

The Olivetti M10 entry is a hidden development sheet while the machine is
brought up. It appears in the collection only when BluMach starts with the
environment variable `BLUMACH_CATALOG_DEVELOPMENT=1`. For automated checks,
`BLUMACH_UNATTENDED=1` reports CoreMach startup errors on standard error
instead of in a dialog.

With a local copy of the M10 firmware, the opt-in check of the
`coremach-bridge` test runs it to the firmware's main menu:

```text
BLUMACH_TEST_ROM_DIR=<ROM folder> ctest --test-dir <build-directory> -R coremach-bridge
```

## Distribution boundary

- Official source archives, binaries and CI artifacts must contain no
  proprietary firmware.
- Local firmware, disk images and restricted research material must not be
  committed to the BluMach repository.
- A separate repository or download does not make firmware redistributable.
- Firmware may be published by BluMach only when an explicit open licence,
  public-domain status or documented permission covers redistribution.
- Checksums and provenance records may be public when they contain no protected
  binary content and disclose no restricted source.

The release package audit checks program structure and startup without firmware.
Machine validation that requires local firmware is performed separately and its
public results must not include the firmware bytes.
