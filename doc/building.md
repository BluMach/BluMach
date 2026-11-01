# Building BluMach

BluMach supports 64-bit hosts and Qt 6. CMake 3.21 or newer and Ninja are
required. The GitHub Actions workflows are the canonical, executable lists of
platform packages:

- [Windows with MSYS2 UCRT64](../.github/workflows/cmake_windows_msys2.yml)
- [Linux](../.github/workflows/cmake_linux.yml)
- [macOS](../.github/workflows/cmake_macos.yml)

After installing the dependencies for the host, a typical Qt 6 development
build is:

```text
cmake -S . -B build --preset development -DNEW_DYNAREC=ON -DQT=ON -DSTATIC_BUILD=OFF
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Use the `regular` preset for a release build. Qt 5 and 32-bit hosts are not
supported BluMach configurations.

## CoreMach engine

Machines whose catalogue profile names the CoreMach engine are run by
[CoreMach](https://github.com/Palimbit/CoreMach), included as the pinned Git
submodule `external/coremach`. Clone with `--recurse-submodules`, or fetch it
in an existing checkout with:

```text
git submodule update --init external/coremach
```

The `BLUMACH_COREMACH` CMake option controls the bridge. The default `AUTO`
builds it when the submodule is present and otherwise leaves it out, so
86Box-based machines build exactly as before. `ON` makes a missing submodule a
configuration error, and `OFF` disables the bridge.

On Windows, configure and build inside the MSYS2 UCRT64 environment. Launch the
result through `tools/run-blumach-local.ps1`; the launcher supplies the UCRT64
runtime and accepts `-VmPath`, `-LogPath` and the workspace ROM directory.

Installed artifacts can be checked without firmware using:

```text
python tools/package_audit.py --help
```

The published Windows package of each version comes from a stricter wrapper,
first written for the private `0.1.0-rc.1` reference build. It builds, tests,
installs, audits and produces a file-by-file manifest plus an archive SHA-256
from a clean commit:

```text
powershell -ExecutionPolicy Bypass -File tools/build-private-rc.ps1 -Version <version>
```

A published package always includes CoreMach. The wrapper does both steps
itself; a package built by hand must do the same:

1. Check out the pinned submodules before configuring:
   `git submodule update --init --recursive`.
2. Configure with `-D BLUMACH_COREMACH=ON`, so a missing submodule stops the
   build instead of leaving the CoreMach machines out.

The manifest records the BluMach commit and, as `source.coremach_commit`, the
`external/coremach` commit the package was built with.

See [the private RC validation record](releases/0.1.0-rc.1.md) for the evidence
that must be captured before portable-engine work begins.

Never add a local ROM directory, guest media or restricted research material to
a build artifact or commit.
