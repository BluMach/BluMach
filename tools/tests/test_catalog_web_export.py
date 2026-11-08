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

    def test_hidden_entries_and_their_texts_stay_out(self) -> None:
        catalog, _ = catalog_web_export.catalog_builder.assemble(ROOT / "src/qt/catalog/source")
        hidden = {p["id"] for p in catalog["products"] if p.get("visible") is not True}
        hidden_families = {f["id"] for f in catalog["families"] if f.get("visible") is not True}
        self.assertTrue(hidden)
        exported = {p["id"] for p in self.catalog["products"]}
        self.assertFalse(hidden & exported)
        self.assertFalse(hidden_families & {f["id"] for f in self.catalog["families"]})
        for name, data in self.files.items():
            if name.endswith(".json"):
                text = data.decode("utf-8")
                for entity_id in hidden | hidden_families:
                    self.assertNotIn(f'"{entity_id}"', text, (name, entity_id))
                    self.assertNotIn(f'.{entity_id}.', text, (name, entity_id))
        for locale in self.manifest["locales"]:
            keys = json.loads(self.files[f"locales/{locale}.json"])
            for product_id in hidden:
                self.assertFalse([k for k in keys if k.startswith(f"product.{product_id}.")], product_id)
            for family_id in hidden_families:
                self.assertFalse([k for k in keys if k.startswith(f"family.{family_id}.")], family_id)

    def test_exported_entries_say_they_are_visible(self) -> None:
        for kind in ("manufacturers", "families", "products"):
            for entity in self.catalog[kind]:
                self.assertIs(True, entity.get("visible"), (kind, entity["id"]))

    def test_only_explicitly_visible_entries_count(self) -> None:
        self.assertTrue(catalog_web_export.visible({"visible": True}))
        self.assertFalse(catalog_web_export.visible({}))
        self.assertFalse(catalog_web_export.visible({"visible": False}))
        self.assertFalse(catalog_web_export.visible({"development": True}))

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

    def test_variant_groups_keep_every_visible_reference(self) -> None:
        self.assertNotIn("variant-groups.json", self.files)
        preview = catalog_web_export.export(
            ROOT / "src/qt/catalog/source", ROOT / "src/qt_resources.qrc",
            "1.2.3-test", "0" * 40, include_variant_groups=True)
        presentation = json.loads(preview.pop("variant-groups.json"))
        self.assertEqual(self.files, preview)
        self.assertEqual("blumach-variant-groups-v1", presentation["schema"])
        products = {p["id"]: p for p in self.catalog["products"]}
        variants = [variant for group in presentation["groups"] for variant in group["variants"]]
        self.assertEqual(11, len(presentation["groups"]))
        self.assertEqual(56, len(variants))
        self.assertEqual(len(variants), len(set(variants)))
        for group in presentation["groups"]:
            for variant in group["variants"]:
                self.assertEqual(group["family_id"], products[variant]["family_id"])
        self.assertNotIn("groups", self.catalog)

    def test_legacy_press_keeps_citation_text_in_the_web_export(self) -> None:
        source, _ = catalog_web_export.catalog_builder.assemble(ROOT / "src/qt/catalog/source")
        original = next(p for p in source["products"] if p["id"] == "schneider-386-professional-340")
        product = next(p for p in self.catalog["products"] if p["id"] == original["id"])
        entries = {e["value_key"]: e for s in product["technical"] for e in s["entries"]}
        legacy = [e for s in original["technical"] for e in s["entries"]
                  if e.get("url", "").startswith(catalog_web_export.LEGACY_SCHNEIDER_PRESS)]
        self.assertTrue(legacy)
        for entry in legacy:
            self.assertNotIn("url", entries[entry["value_key"]])
            self.assertIn(entry["value_key"], json.loads(self.files["locales/es.json"]))

    def test_archive_bytes_are_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            first, second = Path(directory) / "a.zip", Path(directory) / "b.zip"
            catalog_web_export.write_zip(self.files, first)
            catalog_web_export.write_zip(dict(reversed(list(self.files.items()))), second)
            self.assertEqual(first.read_bytes(), second.read_bytes())


if __name__ == "__main__":
    unittest.main()
