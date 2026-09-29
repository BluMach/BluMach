# Historical collection UI checks

This standalone Qt 6 test target compiles the real collection, catalogue, skin
and icon sources. It uses an in-memory settings adapter and a locale adapter;
it does not read user preferences, create VMs, require ROMs or run guest code.

From the repository root, with the Qt 6/compiler runtime on PATH:

```sh
cmake -S tests/qt -B build/collection-tests -G Ninja
cmake --build build/collection-tests
ctest --test-dir build/collection-tests --output-on-failure
```

On Windows use MSYS2 **UCRT64**, as for BluMach itself. Qt Test and LinguistTools
are required. CTest sets `QT_QPA_PLATFORM=offscreen`; no desktop setting changes
are needed. The test covers light and neutral dark palettes (including the
application's dark stylesheet), five locales, and 680/860/1280-pixel widths.

Checks include filter visibility and counts, zero-result creation safety,
keyboard navigation, layout bounds, scroll reset and secondary-text contrast.
Set `BLUMACH_UI_SCREENSHOTS` to an absolute output directory to also save actual
widget renders for visual review, including brand and product selection.
These renders do not validate the native Windows title bar or the full VM
manager shell. The catalogue data comes from this branch, not unmerged machine
review branches.
