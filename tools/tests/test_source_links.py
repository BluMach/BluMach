"""Sources on other people's sites are cited through their pages, not their files."""

from __future__ import annotations

from pathlib import Path
import re
import sys
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import catalog_audit  # noqa: E402

URL = re.compile(r"https?://[^\s)\"'<>`\]|]+")


class SourceLinkTests(unittest.TestCase):
    def test_third_party_files_are_rejected(self) -> None:
        for url in (
            "https://www.minuszerodegrees.net/manuals/Toshiba/Other/T3200.pdf",
            "https://theretroweb.com/motherboard/bios/board-1-02.bin",
            "https://forum.example.org/index.php?file-download/175186/",
            "https://files.scene.org/get/resources/music/trackers/ast.zip",
        ):
            self.assertTrue(catalog_audit.links_to_third_party_file(url), url)

    def test_pages_and_manufacturer_documents_are_accepted(self) -> None:
        for url in (
            "https://www.minuszerodegrees.net/manuals/Toshiba/Toshiba.htm",
            "https://www.ti.com/lit/ds/symlink/tl16c451.pdf",
            "https://files.scene.org/view/resources/music/trackers/ast.zip",
            "https://www.vogonsdrivers.com/getfile.php?fileid=383&menustate=0",
        ):
            self.assertFalse(catalog_audit.links_to_third_party_file(url), url)

    def test_catalogue_rejects_a_third_party_file(self) -> None:
        errors: list[str] = []
        catalog_audit.validate_urls({"entries": [{"url": "https://example.org/manual.pdf"}]}, errors)
        self.assertTrue(any("not to the file" in error for error in errors))

    def test_public_documents_link_to_pages(self) -> None:
        offenders = []
        files = [ROOT / "README.md", *sorted((ROOT / "doc").rglob("*"))]
        for path in files:
            if path.suffix not in {".md", ".yaml", ".yml", ".json"} or not path.is_file():
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                for url in URL.findall(line):
                    if catalog_audit.links_to_third_party_file(url.rstrip(".,;:")):
                        offenders.append(f"{path.relative_to(ROOT)}:{number}: {url}")
        self.assertEqual([], offenders)


if __name__ == "__main__":
    unittest.main()
