# SPDX-License-Identifier: GPL-2.0-or-later
"""Prevent release-selection gaps for catalogued and uncatalogued extensions."""

from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import catalog_builder


class MachineReleaseSelectionTests(unittest.TestCase):
    def setUp(self):
        table = (ROOT / "src/machine/machine_table.c").read_text(encoding="utf-8")
        self.managed = set(re.findall(
            r'\.internal_name\s*=\s*"([^"]+)"\s*,\s*'
            r'\.blumach_release_managed\s*=\s*1\s*,', table))
        self.catalog, _ = catalog_builder.assemble(ROOT / "src/qt/catalog/source")

    def test_catalogued_extensions_follow_release_visibility(self):
        # PC1512 remains inherited even though BluMach improved its emulation.
        ids = {p["emulator_machine_id"] for p in self.catalog["platforms"]
               if p.get("emulator_machine_id") and p["emulator_machine_id"] != "pc1512"}
        self.assertFalse(ids - self.managed, f"Extensions without release ownership: {ids - self.managed}")

    def test_uncatalogued_extensions_are_also_managed(self):
        self.assertTrue({"olivetti_m240", "olivetti_m211v", "olivetti_m290sp",
                         "olivetti_ba2142"} <= self.managed)

    def test_inherited_models_are_not_release_managed(self):
        self.assertFalse({"pc1512", "m24", "m290", "t1000", "t1200", "ibmxt"} & self.managed)


if __name__ == "__main__":
    unittest.main()
