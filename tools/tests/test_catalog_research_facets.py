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
        common = Path(__file__).resolve().parents[2] / 'src/qt/catalog/source/common'
        facets = catalog_builder.read_json(common / 'catalog.json')['filter_facets']
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'research-facets.json'
            source.write_text(
                '{"ta-pending":{"documentation":"documented","firmware_custody":"partial",'
                '"test_destinations":["bogus"]}}', encoding='utf-8'
            )
            with self.assertRaisesRegex(ValueError, 'invalid test_destinations'):
                catalog_builder.load_research_facets(source, facets)
