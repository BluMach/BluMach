# SPDX-License-Identifier: GPL-2.0-or-later
"""Prevent release-selection gaps for catalogued and uncatalogued extensions."""

from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import catalog_builder


# Native machines are selectable independently of the BluMach release list.
BASE_EMULATOR_MODELS = {"pc1512", "m19", "m24", "m290", "t1000", "t1200", "ibmxt"}


class MachineReleaseSelectionTests(unittest.TestCase):
    def setUp(self):
        table = (ROOT / "src/machine/machine_table.c").read_text(encoding="utf-8")
        self.managed = set(re.findall(
            r'\.internal_name\s*=\s*"([^"]+)"\s*,\s*'
            r'\.blumach_release_managed\s*=\s*1\s*,', table))
        self.catalog, _ = catalog_builder.assemble(ROOT / "src/qt/catalog/source")

    def test_catalogued_extensions_follow_release_visibility(self):
        ids = {p["emulator_machine_id"] for p in self.catalog["platforms"] if p.get("emulator_machine_id")}
        extensions = ids - BASE_EMULATOR_MODELS
        self.assertFalse(extensions - self.managed,
                         f"Extensions without release ownership: {extensions - self.managed}")

    def test_uncatalogued_extensions_are_also_managed(self):
        self.assertTrue({"olivetti_m240", "olivetti_m211v", "olivetti_m290sp",
                         "olivetti_ba2142"} <= self.managed)

    def test_base_emulator_models_are_not_release_managed(self):
        self.assertFalse(BASE_EMULATOR_MODELS & self.managed)


if __name__ == "__main__":
    unittest.main()
