"""Permit pinned historical citations without allowing release download links."""

import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location(
    "release_identity", Path(__file__).resolve().parents[1] / "check_release_identity.py"
)
CHECK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECK)


class HistoricalCitationTests(unittest.TestCase):
    source = "src/qt/catalog/source/machines/amstrad/amstrad-pc5086/machine.json"
    prefix = "https://github.com/86Box/" + "roms"
    pinned = prefix + "/tree/" + "a" * 40 + "/machines/pc5086"

    def test_pinned_catalogue_citation(self):
        self.assertNotIn(self.prefix, CHECK.identity_line(self.source, '"url": "' + self.pinned + '"'))

    def test_pinned_preservation_citation(self):
        self.assertNotIn(self.prefix, CHECK.identity_line("doc/preservation/machines/example.md", "[source](" + self.pinned + ")."))

    def test_release_files_still_reject_same_url(self):
        self.assertIn(self.prefix, CHECK.identity_line("README.md", self.pinned))

    def test_unpinned_or_collection_urls_remain_rejected(self):
        for url in [self.prefix, self.prefix + "/tree/master/machines/pc5086", self.prefix + "/tree/abc123/machines/pc5086", self.prefix + "/tree/" + "a" * 40]:
            with self.subTest(url=url):
                self.assertIn(self.prefix, CHECK.identity_line(self.source, url))

    def test_files_and_query_suffixes_remain_rejected(self):
        for suffix in ["/sys_rom.bin", "?download=1", "/../../other"]:
            with self.subTest(suffix=suffix):
                self.assertIn(self.prefix, CHECK.identity_line(self.source, self.pinned + suffix))

    def test_other_obsolete_links_on_same_line_are_kept(self):
        line = self.pinned + '" ' + self.prefix
        self.assertEqual(CHECK.identity_line(self.source, line).count(self.prefix), 1)


if __name__ == "__main__":
    unittest.main()
