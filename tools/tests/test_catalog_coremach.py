#!/usr/bin/env python3

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import catalog_audit  # noqa: E402
import catalog_builder  # noqa: E402


def setting(key, value, section="CoreMach"):
    return {"section": section, "key": key, "value": value}


def coremach_catalog():
    return {
        "schema": catalog_audit.CATALOG_SCHEMA,
        "filter_facets": [],
        "manufacturers": [{"id": "maker", "visible": True}],
        "families": [
            {"id": "family", "manufacturer_id": "maker", "visible": True},
            {"id": "notebooks", "manufacturer_id": "maker",
             "visible": False, "development": True},
        ],
        "platforms": [
            {"id": "pc-platform", "emulator_machine_id": "pc"},
            {"id": "m10-platform", "engine": "coremach",
             "coremach_system_id": "olivetti-m10", "architecture": "80C85"},
        ],
        "products": [
            {"id": "pc", "manufacturer_id": "maker", "family_id": "family",
             "platform_id": "pc-platform", "status": "experimental", "visible": True},
            {"id": "m10", "manufacturer_id": "maker", "family_id": "notebooks",
             "platform_id": "m10-platform", "status": "experimental",
             "visible": False, "development": True,
             "creation": {
                 "fields": [{
                     "id": "memory", "default": "32",
                     "choices": [
                         {"id": "8", "set": [setting("ram_kib", 8)]},
                         {"id": "32", "set": [setting("ram_kib", 32)]},
                     ],
                 }],
                 "configuration": [
                     {"section": "CoreMach", "values": {"language": 2, "rtc_time": 0}},
                 ],
             }},
        ],
    }


def audit(catalog):
    errors: list[str] = []
    catalog_audit.validate_catalog(catalog, errors)
    return errors


def platform(catalog, platform_id):
    return next(item for item in catalog["platforms"] if item["id"] == platform_id)


def product(catalog, product_id):
    return next(item for item in catalog["products"] if item["id"] == product_id)


class CoreMachCatalogTests(unittest.TestCase):
    def test_coremach_platform_and_profile_are_accepted(self) -> None:
        self.assertEqual([], audit(coremach_catalog()))

    def test_platform_without_engine_defaults_to_86box(self) -> None:
        catalog = coremach_catalog()
        self.assertNotIn("engine", platform(catalog, "pc-platform"))
        platform(catalog, "pc-platform")["engine"] = "86box"
        self.assertEqual([], audit(catalog))

    def test_unknown_engine_is_rejected(self) -> None:
        catalog = coremach_catalog()
        platform(catalog, "m10-platform")["engine"] = "mame"
        self.assertTrue(any(".engine must be one of" in error for error in audit(catalog)))

    def test_coremach_platform_requires_known_system(self) -> None:
        for system_id in (None, "olivetti-m20"):
            catalog = coremach_catalog()
            if system_id is None:
                del platform(catalog, "m10-platform")["coremach_system_id"]
            else:
                platform(catalog, "m10-platform")["coremach_system_id"] = system_id
            self.assertTrue(
                any("coremach_system_id must name a supported" in error
                    for error in audit(catalog)), system_id)

    def test_coremach_platform_rejects_86box_machine_id(self) -> None:
        catalog = coremach_catalog()
        platform(catalog, "m10-platform")["emulator_machine_id"] = "olivetti_m10"
        self.assertTrue(any("must not define emulator_machine_id" in error
                            for error in audit(catalog)))

    def test_86box_platform_rejects_coremach_system(self) -> None:
        catalog = coremach_catalog()
        platform(catalog, "pc-platform")["coremach_system_id"] = "olivetti-m10"
        self.assertTrue(any("requires engine 'coremach'" in error for error in audit(catalog)))

    def test_coremach_profile_only_writes_its_section(self) -> None:
        catalog = coremach_catalog()
        creation = product(catalog, "m10")["creation"]
        creation["configuration"].append({"section": "Machine", "values": {"mem_size": 32}})
        self.assertTrue(any("may only write the [CoreMach] section" in error
                            for error in audit(catalog)))

    def test_coremach_profile_rejects_unknown_settings_and_values(self) -> None:
        cases = {
            "does not accept setting 'system'": setting("system", 1),
            "'ram_kib' must be one of": setting("ram_kib", 64),
            "'language' must be one of": setting("language", "it"),
            "'rtc_time' must be one of": setting("rtc_time", True),
        }
        for message, bad_setting in cases.items():
            catalog = coremach_catalog()
            choice = product(catalog, "m10")["creation"]["fields"][0]["choices"][0]
            choice["set"].append(bad_setting)
            self.assertTrue(any(message in error for error in audit(catalog)), message)

    def test_coremach_profile_rejects_generated_files(self) -> None:
        catalog = coremach_catalog()
        creation = product(catalog, "m10")["creation"]
        creation["generated_files"] = [{"path": "disk.img", "size": 1024}]
        self.assertTrue(any("must not generate files" in error for error in audit(catalog)))

    def test_86box_profiles_are_not_restricted_to_coremach_settings(self) -> None:
        catalog = coremach_catalog()
        product(catalog, "pc")["creation"] = copy.deepcopy(
            {"fields": [], "configuration": [{"section": "Machine", "values": {"mem_size": 640}}]})
        self.assertEqual([], audit(catalog))

    def test_development_entries_must_stay_hidden(self) -> None:
        catalog = coremach_catalog()
        product(catalog, "m10")["visible"] = True
        errors = audit(catalog)
        self.assertTrue(any("development product 'm10' must not be visible" in error
                            for error in errors))

    def test_development_flag_must_be_boolean(self) -> None:
        catalog = coremach_catalog()
        product(catalog, "m10")["development"] = "yes"
        self.assertTrue(any("'m10'.development must be a boolean" in error
                            for error in audit(catalog)))

    def test_development_product_requires_a_shown_family(self) -> None:
        catalog = coremach_catalog()
        catalog["families"][1]["development"] = False
        self.assertTrue(any("requires a visible or development family" in error
                            for error in audit(catalog)))

    def test_released_catalogue_has_no_development_entries(self) -> None:
        source = Path(__file__).resolve().parents[2] / "src" / "qt" / "catalog" / "source"
        catalog, _ = catalog_builder.assemble(source)
        for collection in ("manufacturers", "families", "products"):
            for entity in catalog[collection]:
                if entity.get("development"):
                    self.assertFalse(entity.get("visible", False), entity["id"])

    def test_builder_passes_coremach_platform_through(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)

            def write(relative, value):
                path = source / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps(value), encoding="utf-8")

            write("common/catalog.json", {
                "source_schema": catalog_builder.SOURCE_SCHEMA,
                "output_schema": catalog_audit.CATALOG_SCHEMA,
                "filter_facets": []})
            write("common/locales/en.json", {"status.experimental": "Experimental"})
            write("manufacturers/maker/manufacturer.json",
                  {"order": 0, "manufacturer": {"id": "maker", "visible": True}})
            write("manufacturers/maker/locales/en.json", {})
            write("families/maker/notebooks/family.json",
                  {"order": 0, "family": {"id": "notebooks", "manufacturer_id": "maker",
                                          "visible": False, "development": True}})
            write("families/maker/notebooks/locales/en.json", {})
            m10 = product(coremach_catalog(), "m10")
            m10["family_id"] = "notebooks"
            write("machines/maker/m10/machine.json", {
                "order": 0, "platform_order": 0,
                "machine": m10, "platform": platform(coremach_catalog(), "m10-platform")})
            write("machines/maker/m10/locales/en.json", {})

            catalog, translations = catalog_builder.assemble(source)
            self.assertEqual("coremach", catalog["platforms"][0]["engine"])
            self.assertEqual("olivetti-m10", catalog["platforms"][0]["coremach_system_id"])
            self.assertEqual([], catalog_builder.validate(catalog, translations))


if __name__ == "__main__":
    unittest.main()
