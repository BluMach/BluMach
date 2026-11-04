#!/usr/bin/env python3

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import catalog_audit  # noqa: E402


class CatalogAuditTests(unittest.TestCase):
    def test_family_comparison_requires_one_cell_per_column(self):
        table = {"title_key": "family.test.table", "columns": [
            {"id": "model", "label_key": "family.test.model"},
            {"id": "notes", "label_key": "family.test.notes"}],
            "rows": [{"id": "test", "cells": [{"value": "Test"}]}]}
        errors = []
        catalog_audit.validate_family_comparison("test", {"comparison_table": table}, errors)
        self.assertTrue(any("one cell per column" in error for error in errors))

    def test_family_comparison_accepts_literals_and_localized_cells(self):
        table = {"title_key": "family.test.table", "columns": [
            {"id": "model", "label_key": "family.test.model"},
            {"id": "notes", "label_key": "family.test.notes"}],
            "rows": [{"id": "test", "cells": [
                {"value": "Test"}, {"value_key": "family.test.content"}]}]}
        errors = []
        catalog_audit.validate_family_comparison("test", {"comparison_table": table}, errors)
        self.assertEqual([], errors)
        table["rows"][0]["cells"][0]["value_key"] = "family.test.content"
        catalog_audit.validate_family_comparison("test", {"comparison_table": table}, errors)
        self.assertTrue(any("exactly one" in error for error in errors))

    def test_duplicate_json_key_is_rejected(self) -> None:
        with self.assertRaisesRegex(catalog_audit.DuplicateKeyError, "duplicate JSON key 'same'"):
            catalog_audit._unique_object([("same", "first"), ("same", "second")])

    def test_locale_placeholder_mismatch_is_rejected(self) -> None:
        locales = {
            "en": {"message": "Machine %1 has %2 MB"},
            "es": {"message": "La máquina %1 tiene memoria"},
        }
        errors: list[str] = []

        catalog_audit.validate_translation_sets(locales, {"message"}, errors)

        self.assertTrue(any("placeholders for 'message'" in error for error in errors))

    def test_locale_key_parity_is_required(self) -> None:
        locales = {
            "en": {"first": "First", "second": "Second"},
            "fr": {"first": "Premier"},
        }
        errors: list[str] = []

        catalog_audit.validate_translation_sets(locales, set(), errors)

        self.assertIn("fr.json: missing English key 'second'", errors)

    def test_implementation_document_rejects_paths(self) -> None:
        self.assertIsNone(
            catalog_audit.IMPLEMENTATION_DOCUMENT.fullmatch(
                "../../private/machine-implementation.md"
            )
        )
        self.assertIsNotNone(
            catalog_audit.IMPLEMENTATION_DOCUMENT.fullmatch(
                "machine-implementation.md"
            )
        )

    def test_creation_rejects_unavailable_default(self) -> None:
        errors: list[str] = []
        product = {
            "creation": {
                "fields": [{
                    "id": "memory",
                    "default": "4mb",
                    "choices": [{"id": "4mb", "status": "unavailable"}],
                }],
                "configuration": [],
            }
        }

        catalog_audit.validate_creation("example", product, errors)

        self.assertTrue(any("must not reference an unavailable choice" in error for error in errors))

    def test_creation_accepts_valid_field(self) -> None:
        errors: list[str] = []
        product = {
            "creation": {
                "fields": [{
                    "id": "memory",
                    "default": "512",
                    "choices": [{
                        "id": "512",
                        "status": "validated",
                        "set": [{"section": "Machine", "key": "mem_size", "value": 512}],
                    }],
                }],
                "configuration": [{"section": "Machine", "values": {"mem_size": 512}}],
            }
        }

        catalog_audit.validate_creation("example", product, errors)

        self.assertEqual([], errors)

    def test_media_requires_explicit_kind_and_resource(self) -> None:
        errors: list[str] = []
        catalog_audit.validate_media(
            "example", {"media": {"kind": "image", "resource": "example.jpg"}}, errors
        )

        self.assertEqual(3, len(errors))

    def test_media_accepts_declared_concept_illustration(self) -> None:
        errors: list[str] = []
        catalog_audit.validate_media(
            "example",
            {"media": {
                "kind": "concept_illustration",
                "resource": ":/blumach/catalog/images/example.jpg",
                "label_key": "media.kind.concept_illustration",
            }},
            errors,
        )

        self.assertEqual([], errors)

    def test_photograph_requires_credit_license_and_source(self) -> None:
        errors: list[str] = []
        catalog_audit.validate_media(
            "example",
            {"media": {
                "kind": "photograph",
                "resource": ":/blumach/catalog/images/example-photo.jpg",
                "label_key": "media.kind.photograph",
                "license": "All rights reserved",
                "source": "http://example.org/file",
            }},
            errors,
        )

        self.assertEqual(3, len(errors))

    def test_photograph_accepts_credited_free_license(self) -> None:
        errors: list[str] = []
        catalog_audit.validate_media(
            "example",
            {"media": {
                "kind": "photograph",
                "resource": ":/blumach/catalog/images/example-photo.jpg",
                "label_key": "media.kind.photograph",
                "credit": "Example Author",
                "license": "CC BY-SA 4.0",
                "source": "https://commons.wikimedia.org/wiki/File:Example.jpg",
            }},
            errors,
        )

        self.assertEqual([], errors)

    def test_blumach_emulation_is_required_with_a_platform(self) -> None:
        for value in (None, "ported"):
            errors: list[str] = []
            product = {"platform_id": "test-platform"}
            if value is not None:
                product["blumach_emulation"] = value
            catalog_audit.validate_blumach_emulation("test", product, errors)
            self.assertTrue(any("blumach_emulation must be one of" in error for error in errors))

    def test_blumach_emulation_accepts_known_values(self) -> None:
        for value in ("implemented", "improved", "inherited"):
            errors: list[str] = []
            catalog_audit.validate_blumach_emulation(
                "test", {"platform_id": "test-platform", "blumach_emulation": value}, errors)
            self.assertEqual([], errors)

    def test_inherited_machine_has_no_implementation_document(self) -> None:
        product = {"platform_id": "test-platform", "blumach_emulation": "inherited",
                   "implementation": {"document": "test-implementation.md", "language": "en"}}
        errors: list[str] = []
        catalog_audit.validate_blumach_emulation("test", product, errors)
        self.assertTrue(any("no implementation document" in error for error in errors))
        product["blumach_emulation"] = "improved"
        errors.clear()
        catalog_audit.validate_blumach_emulation("test", product, errors)
        self.assertEqual([], errors)

    def test_research_sheet_has_no_blumach_emulation(self) -> None:
        errors: list[str] = []
        catalog_audit.validate_blumach_emulation("test", {"blumach_emulation": "implemented"}, errors)
        self.assertTrue(any("requires a platform" in error for error in errors))

    def test_visible_product_requires_visible_parents(self) -> None:
        errors: list[str] = []

        catalog_audit.validate_visibility(
            {"maker": {"visible": True}},
            {"family": {"manufacturer_id": "maker"}},
            {"model": {
                "manufacturer_id": "maker",
                "family_id": "family",
                "visible": True,
            }},
            errors,
        )

        self.assertIn(
            "catalog.json: visible product 'model' requires visible family 'family'",
            errors,
        )

    def test_visibility_must_be_boolean(self) -> None:
        errors: list[str] = []

        catalog_audit.validate_visibility(
            {"maker": {"visible": "yes"}}, {}, {}, errors
        )

        self.assertEqual(
            ["catalog.json: manufacturer 'maker'.visible must be a boolean"],
            errors,
        )


if __name__ == "__main__":
    unittest.main()
