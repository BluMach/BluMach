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

## Publishing a version

A version is published by pushing its tag, `v<version>`, on the commit whose
release metadata describes it (`python tools/release_version.py check`). The
tag starts the [Release workflow](../.github/workflows/release.yml), which:

1. checks that the tag matches the release metadata;
2. runs `tools/build-private-rc.ps1 -Version <version>` in MSYS2 UCRT64 on
   `windows-2022`, which builds, tests, installs, audits and writes the
   manifest and the archive SHA-256;
3. exports the public part of the catalogue for the website with
   `tools/catalog_web_export.py` (see below);
4. attests the provenance of the package ZIP and the catalogue ZIP with
   GitHub artifact attestations;
5. uploads the package ZIP, its SHA-256 file, the manifest and the catalogue
   ZIP as a run artifact;
6. creates a **draft** GitHub release with those four files and, as notes,
   `doc/releases/<version>.md` when it exists.

The workflow never publishes: review the draft and publish it by hand. It can
also be started from the Actions tab with a `version` and `dry_run`; a dry run,
the default, stops after the run artifact, creates no release and is not
attested. A manual run without `dry_run` must use `master`.

The provenance of a downloaded package can be checked with
`gh attestation verify BluMach-<version>-Windows-x86_64.zip --repo BluMach/BluMach`.

`BluMach-<version>-catalog-web.zip` feeds the machine pages of the website.
It holds `manifest.json` (version, tag and commit), `catalog.json` with the
visible manufacturers, families and machines, the five locale packs and the
images the machines use. Creation templates and emulator identifiers stay out:
they only matter to the application. To produce it for a checkout:

```text
python -B tools/catalog_web_export.py --version <version> --commit <commit> --output catalog-web.zip
```

The same script still works locally to test a package before tagging. It
expects MSYS2 at `C:\msys64`; pass `-Msys2Root` for another location:

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
