#!/usr/bin/env python3

import json
from pathlib import Path
import tempfile
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import catalog_builder  # noqa: E402


class CatalogBuilderTests(unittest.TestCase):
    def test_bundle_locale_key_mismatch_is_rejected(self) -> None:
        locales = {
            "en": {"first": "First", "second": "Second"},
            "es": {"first": "Primero"},
        }

        with self.assertRaisesRegex(ValueError, "es.json key mismatch"):
            catalog_builder.validate_bundle_translations(locales, "test bundle")

    def test_duplicate_translation_ownership_is_rejected(self) -> None:
        target = {"en": {"product.first.summary": "First"}}
        incoming = {"en": {"product.first.summary": "Duplicate"}}

        with self.assertRaisesRegex(ValueError, "duplicate en translation key"):
            catalog_builder.merge_translations(target, incoming, Path("second"))

    def test_qrc_preserves_runtime_aliases(self) -> None:
        qrc = catalog_builder.render_qrc(
            ["en", "es"], ["toshiba-t5100-implementation.md"]
        )

        self.assertIn('prefix="/blumach/catalog"', qrc)
        self.assertIn('alias="catalog.json"', qrc)
        self.assertIn('alias="locales/es.json"', qrc)
        self.assertIn(
            'alias="documents/toshiba-t5100-implementation.md"', qrc
        )

    def test_variant_groups_reject_hidden_and_repeated_members(self) -> None:
        maker = {"id": "example", "visible": True}
        family = {"id": "range", "manufacturer_id": "example", "visible": True}
        first = {"id": "model-a", "family_id": "range", "visible": True}
        hidden = {"id": "model-b", "family_id": "range", "visible": False}
        catalog = {"manufacturers": [maker], "families": [family], "products": [first, hidden]}
        group = {"id": "variants", "name": "Variants", "family_id": "range",
                 "summary_key": "group.summary", "source_url": "https://example.com/archive"}
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / "common").mkdir()
            for members, message in ((["model-a", "model-b"], "must be visible"),
                                     (["model-a", "model-a"], "more than one position")):
                group["variants"] = members
                (source / "common/variant-groups.json").write_text(json.dumps({
                    "schema": catalog_builder.VARIANT_GROUPS_SCHEMA, "groups": [group],
                }), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    catalog_builder.load_variant_groups(source, catalog, {"en": {"group.summary": "Summary"}})

    def test_desktop_groups_and_locales_require_opt_in(self) -> None:
        catalog = {
            "manufacturers": [{"id": "example", "visible": True}],
            "families": [{"id": "range", "manufacturer_id": "example", "visible": True}],
            "products": [
                {"id": model, "family_id": "range", "visible": True}
                for model in ("model-a", "model-b", "model-c")
            ],
        }
        desktop_group = {
            "id": "desktop-variants",
            "name": "Desktop variants",
            "family_id": "range",
            "summary_key": "desktop.summary",
            "source_url": "https://example.com/archive",
            "variants": ["model-a", "model-b"],
        }
        common_group = {
            **desktop_group,
            "id": "common-variants",
            "summary_key": "common.summary",
            "variants": ["model-b", "model-c"],
        }
        translations = {"en": {"common.summary": "Common summary"}}
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            desktop = source / "desktop"
            (desktop / "locales").mkdir(parents=True)
            catalog_builder.write_json(
                desktop / "locales/en.json", {"desktop.summary": "Desktop summary"}
            )
            catalog_builder.write_json(
                desktop / catalog_builder.VARIANT_GROUPS_FILE,
                {"schema": catalog_builder.VARIANT_GROUPS_SCHEMA, "groups": [desktop_group]},
            )
            self.assertEqual(
                catalog_builder.load_variant_groups(source, catalog, translations)["groups"], []
            )
            self.assertNotIn("desktop.summary", translations["en"])

            desktop_translations = {"en": dict(translations["en"])}
            self.assertEqual(
                catalog_builder.load_variant_groups(
                    source, catalog, desktop_translations, desktop=True
                )["groups"], [desktop_group]
            )
            self.assertEqual(desktop_translations["en"]["desktop.summary"], "Desktop summary")

            catalog_builder.write_json(
                source / "common" / catalog_builder.VARIANT_GROUPS_FILE,
                {"schema": catalog_builder.VARIANT_GROUPS_SCHEMA, "groups": [common_group]},
            )
            with self.assertRaisesRegex(ValueError, "model-b.*more than one position"):
                catalog_builder.load_variant_groups(source, catalog, translations, desktop=True)

    def test_implementation_documents_are_deduplicated(self) -> None:
        catalog = {
            "products": [
                {"implementation": {"document": "machine-implementation.md"}},
                {"implementation": {"document": "machine-implementation.md"}},
                {},
            ]
        }

        self.assertEqual(
            catalog_builder.implementation_documents(catalog),
            ["machine-implementation.md"],
        )

    def test_generated_resource_is_initialized_by_qt_main(self) -> None:
        repository = Path(__file__).resolve().parents[2]
        qt_main = (repository / "src" / "qt" / "qt_main.cpp").read_text(encoding="utf-8")

        self.assertIn("Q_INIT_RESOURCE(blumach_catalog);", qt_main)

    def test_visibility_matches_selected_profiles_and_historical_entries(self) -> None:
        repository = Path(__file__).resolve().parents[2]
        source = repository / "src" / "qt" / "catalog" / "source"
        catalog, _ = catalog_builder.assemble(source)

        visible_manufacturers = {
            item["id"] for item in catalog["manufacturers"] if item.get("visible", False)
        }
        visible_families = {
            item["id"] for item in catalog["families"] if item.get("visible", False)
        }
        visible_products = {
            item["id"] for item in catalog["products"] if item.get("visible", False)
        }

        self.assertEqual({
                "amstrad", "olivetti", "schneider", "toshiba", "trigem", "triumph-adler",
            },
                         visible_manufacturers)
        self.assertEqual(
            {
                "amstrad-acl", "amstrad-alt", "amstrad-anb", "amstrad-integra", "amstrad-mega-pc",
                "amstrad-pc1512", "amstrad-pc1640", "amstrad-pc1x86", "amstrad-pc20-pc200", "amstrad-pc2000",
                "amstrad-pc3000", "amstrad-pc6000", "amstrad-pc7000", "amstrad-pc9000",
                "amstrad-pci-1995", "amstrad-ppc", "amstrad-series-4", "amstrad-series-5",
                "amstrad-series-8", "amstrad-sinclair-apc", "olivetti-m", "olivetti-pcs",
                "triumph-adler-bitsy",
                "olivetti-prodest", "schneider-386-professional", "schneider-48625",
                "triumph-adler-ta1900",
                "triumph-adler-ta10",
                "schneider-amstrad-badges", "schneider-at-desktops", "schneider-d38620",
                "triumph-adler-ta1000",
                "triumph-adler-ta1600",
                "schneider-d48620", "schneider-desktops-1992", "schneider-euro-xt",
                "triumph-adler-bss",
                "schneider-euro486", "schneider-eurosx", "schneider-l38616", "schneider-n38616",
                "schneider-n486slc", "schneider-portable-sx", "schneider-target-pc",
                "triumph-adler-ta1100",
                "triumph-adler-ta20",
                "schneider-tower-at", "schneider-tower386sx", "schneider-towers-1991",
                "triumph-adler-ta1500",
                "schneider-towers-1992", "schneider-vga-at", "toshiba-t-series", "trigem-sx386",
                "triumph-adler-msx3200",
                "triumph-adler-alphatronic-p",
                "triumph-adler-m32",
                "triumph-adler-alphatronic-pc",
                "triumph-adler-alphatronic-x86",
                "triumph-adler-ta100",
                "triumph-adler-dialog-terminals",
                "triumph-adler-dario",
                "triumph-adler-walkstation",
            }, visible_families)
        self.assertEqual(
            {
                "amstrad-acl-386sx120", "amstrad-alt-286", "amstrad-alt-386sx",
                "amstrad-anb-386sx20", "amstrad-anb-386sx40", "amstrad-es8486",
                "amstrad-integra-486", "amstrad-integra-p75", "amstrad-integra-powerpro",
                "ta-bitsy-2", "ta-bitsy-10", "ta-bitsy-20", "ta-bitsy-30",
                "amstrad-lp8486", "amstrad-megapc", "amstrad-megaplus", "amstrad-pc1286",
                "amstrad-pc1386", "amstrad-pc1512", "amstrad-pc1640", "amstrad-pc20", "amstrad-pc2086",
                "amstrad-pc2286", "amstrad-pc2386", "amstrad-pc3086", "amstrad-pc3286",
                "ta-1520", "ta-1540",
                "amstrad-pc3386sx", "amstrad-pc4386sx", "amstrad-pc5086", "amstrad-pc5286",
                "amstrad-pc5386sx", "amstrad-pc6486sx", "amstrad-pc7286", "amstrad-pc7386sx",
                "amstrad-pc7486slc33", "amstrad-pc7486slc80", "amstrad-pc9486", "amstrad-pc9486i",
                "amstrad-pc9555i", "amstrad-ppc512", "amstrad-ppc640", "olivetti-cp486",
                "olivetti-echos-20", "olivetti-echos-20-color", "olivetti-echos-24",
                "olivetti-echos-24c", "olivetti-echos-26c", "olivetti-echos-42",
                "olivetti-echos-43-color", "olivetti-echos-44-color", "olivetti-echos-46-color",
                "olivetti-echos-48-color-plus", "olivetti-echos-p100c", "olivetti-echos-p100d",
                "olivetti-echos-p100e", "olivetti-echos-p120d", "olivetti-echos-p120e",
                "olivetti-echos-p120s", "olivetti-echos-p133c", "olivetti-echos-p133s",
                "olivetti-echos-p75", "olivetti-echos-p90", "olivetti-echos-p90s",
                "olivetti-echos-pro-133-s", "olivetti-echos-pro-150s", "olivetti-echos-pro-166xm",
                "ta-1100", "ta-1100c",
                "ta-20-se", "ta-20-np", "ta-20-compact", "ta-21-mbke", "ta-21-fde", "ta-des", "ta-22", "ta-22b",
                "olivetti-envision-400", "olivetti-envision-p75", "olivetti-laptop-d33",
                "olivetti-laptop-d33-c", "olivetti-laptop-lx4-c", "olivetti-laptop-s20",
                "olivetti-lsx-5010", "olivetti-lsx-5015", "olivetti-lsx-5020", "olivetti-lsx-5025",
                "olivetti-lsx-5025e", "olivetti-lsx-5030", "olivetti-lsx-5050", "olivetti-m10",
                "olivetti-m111", "olivetti-m15", "olivetti-m15-plus", "olivetti-m18p",
                "olivetti-m19", "olivetti-m20", "olivetti-m20-d", "olivetti-m20-st",
                "olivetti-m200", "olivetti-m21", "olivetti-m211", "olivetti-m211v", "olivetti-m22",
                "olivetti-m24", "olivetti-m24-new-p100", "olivetti-m24-new-p133",
                "ta-10-1", "ta-10-2", "ta-10-3", "ta-10-4",
                "ta-1600-20", "ta-1600-20-3", "ta-1600-30",
                "ta-1600-25", "ta-1600-35", "ta-1600-38",
                "olivetti-m24-new-p75", "olivetti-m240", "olivetti-m24sp", "olivetti-m250",
                "olivetti-m250e", "olivetti-m28", "olivetti-m28-20", "olivetti-m28-20c",
                "olivetti-m28-40", "olivetti-m28-40c", "olivetti-m28-sp", "olivetti-m280",
                "olivetti-m290", "olivetti-m290-20", "olivetti-m290-25", "olivetti-m290-30",
                "olivetti-m290s", "olivetti-m290sp", "olivetti-m30", "olivetti-m300-01",
                "olivetti-m300-04", "olivetti-m300-05", "olivetti-m300-10", "olivetti-m300-25",
                "ta-1000", "ta-1069",
                "olivetti-m300-28", "olivetti-m300-if378", "olivetti-m30002", "olivetti-m30002f",
                "olivetti-m30008", "olivetti-m30015", "olivetti-m30030", "olivetti-m30030p",
                "olivetti-m316", "olivetti-m34", "olivetti-m380", "olivetti-m380-40",
                "olivetti-m380-c", "olivetti-m380-c-new", "olivetti-m380-xp1", "olivetti-m380-xp3",
                "olivetti-m380-xp4", "olivetti-m380-xp5", "olivetti-m380-xp7", "olivetti-m380-xp9",
                "olivetti-m380t", "olivetti-m386-25", "olivetti-m4-100", "olivetti-m4-100-s",
                "ta-alphatronic-p92",
                "olivetti-m4-133", "olivetti-m4-133-s", "olivetti-m4-34", "olivetti-m4-40",
                "olivetti-m4-422", "olivetti-m4-422s", "olivetti-m4-424", "olivetti-m4-424s",
                "ta-msx-3220", "ta-msx-3230", "ta-msx-3240",
                "olivetti-m4-432", "olivetti-m4-432s", "olivetti-m4-434", "olivetti-m4-434s",
                "olivetti-m4-452", "olivetti-m4-452s", "olivetti-m4-454", "olivetti-m4-454s",
                "olivetti-m4-46", "olivetti-m4-464", "olivetti-m4-464s", "olivetti-m4-484",
                "olivetti-m4-484s", "olivetti-m4-554", "olivetti-m4-554-s", "olivetti-m4-564",
                "olivetti-m4-564-s", "olivetti-m4-584", "olivetti-m4-584-s", "olivetti-m4-60",
                "olivetti-m4-62", "olivetti-m4-64", "olivetti-m4-65", "olivetti-m4-66",
                "olivetti-m4-75", "olivetti-m4-75-s", "olivetti-m4-82", "olivetti-m4-90",
                "olivetti-m4-90-s", "olivetti-m4-p100", "olivetti-m4-p100-s", "olivetti-m4-p133",
                "olivetti-m4-p133-s", "olivetti-m4-p75", "olivetti-m4-p75-s", "olivetti-m4-p90-s",
                "olivetti-m4-pxx-i", "olivetti-m40", "olivetti-m400-10", "olivetti-m400-40",
                "olivetti-m400-60", "olivetti-m44", "olivetti-m480-10", "olivetti-m480-20",
                "olivetti-m480-30", "olivetti-m480-40", "olivetti-m480-60", "olivetti-m486-esdi",
                "olivetti-m486-scsi", "olivetti-m54", "olivetti-m6-400", "olivetti-m6-420",
                "olivetti-m6-440", "olivetti-m6-450", "olivetti-m6-460", "olivetti-m6-520",
                "olivetti-m6-540", "olivetti-m6-560", "olivetti-m6-620", "olivetti-m6-6200",
                "olivetti-m6-640", "olivetti-m6-640-dp", "olivetti-m6-650", "olivetti-m6-650-dp",
                "olivetti-m6-670", "olivetti-m6-680", "olivetti-m6-680-dp", "olivetti-m6-690",
                "olivetti-m6-690-dp", "olivetti-m6-750", "olivetti-m6-750-s", "olivetti-m6-760",
                "olivetti-m6-760-s", "olivetti-m6-770", "olivetti-m6-770-s", "olivetti-m6-850",
                "olivetti-m6-860", "olivetti-m6-880", "olivetti-m6-950", "olivetti-m60",
                "ta-1900-base",
                "olivetti-m60-2", "olivetti-m60-3", "olivetti-m64", "olivetti-m70",
                "olivetti-m70-2", "olivetti-m70-3", "olivetti-m700-10", "olivetti-modulo-p100-l",
                "ta-bss-300",
                "olivetti-modulo-p120-l", "olivetti-modulo-p133-l", "olivetti-modulo-pro-180",
                "olivetti-netstrada-1000", "olivetti-netstrada-1400", "olivetti-netstrada-2600",
                "olivetti-netstrada-3000", "olivetti-netstrada-3400",
                "olivetti-netstrada-3400-plus", "olivetti-netstrada-5000",
                "olivetti-netstrada-5200", "olivetti-netstrada-5400",
                "olivetti-netstrada-5400-plus", "olivetti-netstrada-5600",
                "olivetti-netstrada-7000", "olivetti-netstrada-7200", "olivetti-netstrada-7400",
                "olivetti-netstrada-7600", "olivetti-netstrada-9400", "olivetti-notebook-a12",
                "olivetti-notebook-s20", "olivetti-notebook-s25", "olivetti-notebook-s25-c",
                "olivetti-notebook-v16", "olivetti-p500", "olivetti-p700", "olivetti-p750",
                "olivetti-p800", "olivetti-pcs-d4-100-en", "olivetti-pcs-d4-100-n",
                "olivetti-pcs-p-100-en", "olivetti-pcs-p-100-n", "olivetti-pcs-p-133-en",
                "olivetti-pcs-p-133-n", "olivetti-pcs-p-75-e-en", "olivetti-pcs-p-75-educator",
                "olivetti-pcs-p-75-n", "olivetti-pcs-p-90-e-en", "olivetti-pcs-p-90-n",
                "olivetti-pcs11", "olivetti-pcs286", "olivetti-pcs286s", "olivetti-pcs30-dx40",
                "olivetti-pcs30-sx40", "olivetti-pcs33", "olivetti-pcs386sx", "olivetti-pcs40-d250",
                "olivetti-pcs40-d266", "olivetti-pcs40-dx33", "olivetti-pcs40-sx25",
                "olivetti-pcs40-sx33", "olivetti-pcs42", "olivetti-pcs42p-d2-50",
                "olivetti-pcs42p-d2-50-e", "olivetti-pcs42p-d2-66", "olivetti-pcs42p-d2-66-e",
                "olivetti-pcs42p-s2-50", "olivetti-pcs42p-s2-50-e", "olivetti-pcs42p-sx-25",
                "olivetti-pcs42p-sx-25-e", "olivetti-pcs42p-sx-33", "olivetti-pcs42p-sx-33-e",
                "olivetti-pcs44", "olivetti-pcs44-c", "olivetti-pcs46c", "olivetti-pcs5120",
                "olivetti-pcs5130", "olivetti-pcs5140", "olivetti-pcs52-e", "olivetti-pcs6120",
                "olivetti-pcs6130", "olivetti-pcs6140", "olivetti-pcs6150", "olivetti-pcs6160",
                "olivetti-pcs86", "olivetti-philos-11", "olivetti-philos-20", "olivetti-philos-22",
                "olivetti-philos-22-color", "olivetti-philos-33", "olivetti-philos-33-color",
                "olivetti-philos-44", "olivetti-philos-44-color", "olivetti-philos-45",
                "olivetti-philos-45-color", "olivetti-philos-46", "olivetti-philos-46-color-plus",
                "olivetti-philos-48", "olivetti-philos-48-color-plus", "olivetti-prodest-pc1",
                "olivetti-prodest-pc128", "olivetti-prodest-pc128s", "olivetti-prodest-pc1hd",
                "olivetti-quaderno", "olivetti-quaderno-33", "olivetti-snx-140",
                "olivetti-snx-140-r", "olivetti-snx-140-s", "olivetti-snx-160",
                "olivetti-snx-160-n", "olivetti-snx-160-r", "olivetti-snx-160-rm",
                "olivetti-snx-160-rs", "olivetti-snx-160-s", "olivetti-snx-160e",
                "olivetti-snx-200-rm", "olivetti-snx-200-rs", "olivetti-snx-400-rm",
                "olivetti-snx-400-rs", "olivetti-snx-460-rm", "olivetti-snx-460-rs",
                "schneider-386-60-16", "schneider-386-professional-160",
                "schneider-386-professional-340", "schneider-386-professional-60",
                "schneider-48625", "schneider-d38620-105", "schneider-d38620-200",
                "schneider-d38620-52", "schneider-d386dx40", "schneider-d386sx25",
                "schneider-d48620-175", "schneider-d48620-320", "schneider-d48620-420",
                "schneider-d486dx33", "schneider-d486sx25", "schneider-euro486-20-105",
                "schneider-euro486-20-40", "schneider-euro486-20-90", "schneider-euroat",
                "schneider-euroat-ii", "schneider-europc", "schneider-europc2",
                "schneider-eurosx16", "schneider-eurosx20", "schneider-euroxt",
                "schneider-l38616-hd60", "schneider-l38616-hd80", "schneider-n38616-40",
                "schneider-n38616-80", "schneider-n38616-86", "schneider-n486slc",
                "schneider-pc1512", "schneider-pc1640", "schneider-pc2640", "schneider-pc2640-2",
                "schneider-portable-sx", "schneider-t38625", "schneider-t386dx40",
                "schneider-t48633", "schneider-t486dx33", "schneider-t486dx50",
                "schneider-targetpc7640", "schneider-tower386sx40", "schneider-tower386sx70",
                "schneider-towerat201", "schneider-towerat202", "schneider-towerat220",
                "schneider-towerat220-mega", "schneider-towerat240", "schneider-towerat260",
                "schneider-vgaat40", "schneider-vgaat70", "sinclair-apc286", "sinclair-apc386sx",
                "sinclair-pc200", "ta-alphatronic-p1", "ta-alphatronic-p2",
                "ta-alphatronic-p2l", "ta-alphatronic-p2s", "ta-alphatronic-p2u",
                "ta-80", "ta-dsd",
                "ta-alphatronic-p3", "ta-alphatronic-p4", "ta-alphatronic-p30", "ta-alphatronic-p40",
                "ta-m32",
                "ta-alphatronic-pc", "ta-alphatronic-pc16",
                "ta-alphatronic-p10", "ta-alphatronic-p10-s", "ta-alphatronic-p50-1", "ta-alphatronic-p50-2", "ta-alphatronic-p60-1", "ta-alphatronic-p60-2", "ta-1700pc",
                "ta-100-2",
                "ta-dario286", "ta-dario386sx", "toshiba-t3200", "toshiba-t5100",
                "ta-l21", "ta-walkstation-386sx", "ta-walkstation-386dx",
                "ta-walkstation-286c", "ta-walkstation-286v", "ta-walkstation-system-sx20",
                "toshiba-t5200", "trigem-sx386m",
            },
            visible_products,
        )

        loader = (repository / "src" / "qt" / "qt_blumach_catalog.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('value(QStringLiteral("visible")).toBool(false)', loader)

    def test_manufacturers_are_listed_alphabetically(self) -> None:
        source = Path(__file__).resolve().parents[2] / "src" / "qt" / "catalog" / "source"
        catalog, _ = catalog_builder.assemble(source)
        names = [item["name"] for item in catalog["manufacturers"]]
        self.assertEqual(names, sorted(names, key=str.casefold))


if __name__ == "__main__":
    unittest.main()
