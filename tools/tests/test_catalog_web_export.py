#!/usr/bin/env python3

import json
from pathlib import Path
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import catalog_web_export  # noqa: E402


ROOT = Path(__file__).resolve().parents[2]


class CatalogWebExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.files = catalog_web_export.export(
            ROOT / "src/qt/catalog/source", ROOT / "src/qt_resources.qrc",
            "1.2.3-test", "0" * 40)
        cls.catalog = json.loads(cls.files["catalog.json"])
        cls.manifest = json.loads(cls.files["manifest.json"])

    def test_manifest_names_the_version_and_its_tag(self) -> None:
        self.assertEqual("blumach-catalog-web-v1", self.manifest["schema"])
        self.assertEqual("v1.2.3-test", self.manifest["tag"])
        self.assertEqual(len(self.catalog["products"]), self.manifest["products"])
        for locale in self.manifest["locales"]:
            self.assertIn(f"locales/{locale}.json", self.files)

    def test_creation_details_stay_in_the_application(self) -> None:
        for product in self.catalog["products"]:
            for field in catalog_web_export.PRIVATE_PRODUCT_FIELDS:
                self.assertNotIn(field, product, product["id"])
        text = self.files["catalog.json"].decode("utf-8")
        self.assertNotIn("emulator_machine_id", text)
        self.assertNotIn("coremach_system_id", text)

    def test_every_image_is_included_with_its_product(self) -> None:
        for product in self.catalog["products"]:
            media = product.get("media")
            if media:
                self.assertTrue(media["resource"].startswith("images/"), product["id"])
                self.assertIn(media["resource"], self.files)
        images = {name for name in self.files if name.startswith("images/")}
        used = {p["media"]["resource"] for p in self.catalog["products"] if p.get("media")}
        self.assertEqual(used, images)

    def test_engineering_note_links_to_the_tagged_document(self) -> None:
        notes = [p["implementation"] for p in self.catalog["products"] if p.get("implementation")]
        self.assertTrue(notes)
        for note in notes:
            self.assertEqual(
                f"https://github.com/BluMach/BluMach/blob/v1.2.3-test/doc/machines/{note['document']}",
                note["url"])

    def test_engine_and_architecture_replace_the_platform(self) -> None:
        m10 = next(p for p in self.catalog["products"] if p["id"] == "olivetti-m10")
        self.assertEqual("coremach", m10["engine"])
        self.assertEqual("80C85", m10["architecture"])

    def test_archive_bytes_are_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            first, second = Path(directory) / "a.zip", Path(directory) / "b.zip"
            catalog_web_export.write_zip(self.files, first)
            catalog_web_export.write_zip(dict(reversed(list(self.files.items()))), second)
            self.assertEqual(first.read_bytes(), second.read_bytes())


if __name__ == "__main__":
    unittest.main()
