#!/usr/bin/env python3

from pathlib import Path
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import catalog_builder  # noqa: E402


class CatalogResearchFacetTests(unittest.TestCase):
    def test_research_facets_keep_state_and_visibility(self) -> None:
        machine = {"status": "research", "visible": False, "facets": {"video": ["vga"]}}
        review = {
            "documentation": "documented",
            "firmware_custody": "partial",
            "test_destinations": ["mame_preliminary", "fpga_sidi", "fpga_mister"],
        }
        catalog_builder.apply_research_facets(machine, {"emulator_machine_id": "test"}, review)
        self.assertEqual(machine, {
            "status": "research", "visible": False, "facets": {
                "video": ["vga"],
                "documentation": ["documented"],
                "firmware_custody": ["partial"],
                "test_destinations": ["fpga", "mame_preliminary", "fpga_sidi", "fpga_mister"],
            },
        })
        self.assertEqual(review["test_destinations"], ["mame_preliminary", "fpga_sidi", "fpga_mister"])

    def test_research_facets_distinguish_unreviewed_and_available(self) -> None:
        machine = {"status": "partial"}
        catalog_builder.apply_research_facets(machine, {"emulator_machine_id": "test"}, None)
        self.assertEqual(machine["facets"], {
            "documentation": ["unreviewed"],
            "firmware_custody": ["unreviewed"],
            "test_destinations": ["blumach"],
        })
        catalog_builder.apply_research_facets(machine, {}, None)
        self.assertEqual(machine["facets"]["test_destinations"], ["unreviewed"])

    def test_pending_rows_are_validated_before_their_machines_merge(self) -> None:
        research = Path(__file__).resolve().parents[2] / "src/qt/catalog/source/desktop/research"
        facets = catalog_builder.read_json(research / "filter-facets.json")
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'research-facets.json'
            source.write_text(
                '{"ta-pending":{"documentation":"documented","firmware_custody":"partial",'
                '"test_destinations":["bogus"]}}', encoding='utf-8'
            )
            with self.assertRaisesRegex(ValueError, 'invalid test_destinations'):
                catalog_builder.load_research_facets(source, facets)

    def test_preservation_filters_are_only_in_desktop_resources(self) -> None:
        source = Path(__file__).resolve().parents[2] / "src/qt/catalog/source"
        common, common_text = catalog_builder.assemble(source)
        desktop, desktop_text = catalog_builder.assemble(source, desktop=True)

        for key in ("documentation", "firmware_custody", "test_destinations"):
            self.assertNotIn(key, {facet["id"] for facet in common["filter_facets"]})
            self.assertIn(key, {facet["id"] for facet in desktop["filter_facets"]})
            for locale in common_text:
                self.assertNotIn(f"filter.facet.{key}", common_text[locale])
                self.assertIn(f"filter.facet.{key}", desktop_text[locale])

        common_products = {machine["id"]: machine for machine in common["products"]}
        desktop_products = {machine["id"]: machine for machine in desktop["products"]}
        p30 = "ta-alphatronic-p30"
        self.assertNotIn("firmware_custody", common_products[p30].get("facets", {}))
        self.assertEqual(desktop_products[p30]["facets"]["firmware_custody"], ["partial"])
        self.assertEqual(desktop_products[p30]["facets"]["test_destinations"], ["mame_preliminary"])
        self.assertEqual(desktop_products[p30]["status"], common_products[p30]["status"])
        self.assertEqual(desktop_products[p30]["visible"], common_products[p30]["visible"])
