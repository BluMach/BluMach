# BluMach fork notice

BluMach is a modified distribution of 86Box, an x86 systems emulator.

- Upstream project: https://github.com/86Box/86Box
- BluMach source: https://github.com/BluMach/BluMach
- License: GNU General Public License, version 2 or later
- License text: `COPYING`

The original copyright notices, authorship records and source-file headers are
retained. BluMach modifications are identified through this repository's Git
history. The BluMach name does not imply endorsement by or affiliation with the
upstream 86Box project.

## CoreMach

BluMach also includes CoreMach, a portable emulation engine extracted from
BluMach and maintained as a separate project. Machines whose catalogue profile
names the CoreMach engine run on it instead of the 86Box core.

- CoreMach source: https://github.com/Palimbit/CoreMach
- License: GNU General Public License, version 2 or later
- License text: `external/coremach/COPYING`
- Location in this repository: the `external/coremach` Git submodule, pinned
  to a specific CoreMach commit

BluMach builds only CoreMach's engine, runtime, components and machine systems.
CoreMach's own frontends and catalogue are not part of BluMach. CoreMach's
copyright notices, authorship records and provenance notice
(`external/coremach/FORK-NOTICE.md`) are retained.

## Distribution

Binary distributions of BluMach must be accompanied by the corresponding
source code, or by a GPL-compliant written offer or equivalent source-access
mechanism, and must include the license and this provenance notice. Firmware,
ROM images, operating systems and other data files are not automatically
covered by the emulator's GPL license; distributors must verify their rights to
ship each such asset separately.

Internal target names, include paths and configuration keys may continue to use
the `86Box` name for source and profile compatibility. They are implementation
identifiers, not a claim that BluMach is an official upstream build.
