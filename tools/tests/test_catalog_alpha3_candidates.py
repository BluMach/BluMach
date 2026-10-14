# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 rtzor, Project BluMach.
"""Keep the selected next-alpha profiles and exclusions explicit."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import catalog_builder


class Alpha3CandidateTests(unittest.TestCase):
    def setUp(self):
        self.catalog, self.locales = catalog_builder.assemble(
            ROOT / "src/qt/catalog/source")
        self.products = {p["id"]: p for p in self.catalog["products"]}

    def values(self, product):
        return {s["section"]: s["values"]
                for s in product["creation"]["configuration"]}

    def test_selected_five_visible_but_pc1512_stays_out(self):
        selected = {"olivetti-pcs286s", "olivetti-prodest-pc1",
                    "olivetti-prodest-pc1hd", "olivetti-m28",
                    "amstrad-pc5286"}
        for product_id in selected:
            with self.subTest(product=product_id):
                self.assertTrue(self.products[product_id].get("visible", False))
                self.assertTrue(self.products[product_id]["creation"])
        self.assertFalse(self.products["amstrad-pc1512"].get("visible", False))
        self.assertFalse(self.products["olivetti-pcs286s-16"].get("visible", False))

    def test_m28_commercial_floppy_is_525_inch_1200kb(self):
        config = self.values(self.products["olivetti-m28"])
        self.assertEqual("525_2hd",
                         config["Floppy and CD-ROM drives"]["fdd_01_type"])
        self.assertEqual("st506_at", config["Storage controllers"]["hdc_1"])
        self.assertEqual("17, 4, 615, 0, mfm",
                         config["Hard disks"]["hdd_01_parameters"])

    def test_pc1hd_keeps_32_sector_xta_not_generic_17(self):
        config = self.values(self.products["olivetti-prodest-pc1hd"])
        self.assertEqual("32, 2, 667, 0, xta",
                         config["Hard disks"]["hdd_01_parameters"])
        self.assertEqual("necv40", config["Machine"]["cpu_family"])

    def test_pc5286_hdd_is_choice_owned_and_status_experimental(self):
        product = self.products["amstrad-pc5286"]
        self.assertEqual("experimental", product["status"])
        self.assertFalse(product["creation"].get("generated_files", []))
        field = next(f for f in product["creation"]["fields"]
                     if f["id"] == "commercial_model")
        choices = {c["id"]: c for c in field["choices"]}
        self.assertFalse(choices["floppy"].get("generated_files", []))
        for choice in ("hd40", "games-pack"):
            self.assertTrue(choices[choice]["generated_files"])
            self.assertEqual("experimental", choices[choice]["status"])

    def test_pcs286s_ti_does_not_select_16mhz_board(self):
        config = self.values(self.products["olivetti-pcs286s"])
        self.assertEqual(12000000, config["Machine"]["cpu_speed"])
        self.assertEqual("v206", config["Olivetti PCS 286S (TI)"]["bios"])


if __name__ == "__main__":
    unittest.main()
