#!/usr/bin/env python3
"""Build deterministic BluMach catalogue resources from sharded sources."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

import catalog_audit


SOURCE_SCHEMA = "blumach-catalog-source-v1"
METADATA_FILES = {
    "manufacturers": "manufacturer.json",
    "families": "family.json",
    "machines": "machine.json",
}


def read_json(path: Path) -> Any:
    errors: list[str] = []
    value = catalog_audit.load_json(path, errors)
    if errors:
        raise ValueError("\n".join(errors))
    return value


def write_text_if_changed(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8")


def write_json(path: Path, value: Any) -> None:
    write_text_if_changed(path, json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def load_locale_directory(path: Path, expected: set[str] | None = None) -> dict[str, dict[str, str]]:
    locale_files = sorted(path.glob("*.json"))
    locales = {locale_file.stem: read_json(locale_file) for locale_file in locale_files}
    if expected is not None and set(locales) != expected:
        missing = sorted(expected - set(locales))
        extra = sorted(set(locales) - expected)
        raise ValueError(f"{path}: locale set mismatch; missing={missing}, extra={extra}")
    for locale, translations in locales.items():
        if not isinstance(translations, dict):
            raise ValueError(f"{path / (locale + '.json')}: root must be an object")
    validate_bundle_translations(locales, path)
    return locales


def validate_bundle_translations(
    locales: dict[str, dict[str, str]], owner: Path | str
) -> None:
    if "en" not in locales:
        raise ValueError(f"{owner}: en.json is required")
    english_keys = set(locales["en"])
    for locale, translations in locales.items():
        keys = set(translations)
        if keys != english_keys:
            missing = sorted(english_keys - keys)
            extra = sorted(keys - english_keys)
            raise ValueError(
                f"{owner}: {locale}.json key mismatch; missing={missing}, extra={extra}"
            )


def merge_translations(
    target: dict[str, dict[str, str]],
    incoming: dict[str, dict[str, str]],
    owner: Path,
) -> None:
    for locale, translations in incoming.items():
        for key, value in translations.items():
            if key in target[locale]:
                raise ValueError(f"{owner}: duplicate {locale} translation key {key!r}")
            target[locale][key] = value


def load_bundles(
    source_dir: Path,
    group: str,
    locales: set[str],
) -> list[tuple[int, str, dict[str, Any], dict[str, dict[str, str]]]]:
    bundles = []
    orders: set[int] = set()
    filename = METADATA_FILES[group]
    for metadata_path in sorted((source_dir / group).glob(f"**/{filename}")):
        wrapper = read_json(metadata_path)
        if not isinstance(wrapper, dict):
            raise ValueError(f"{metadata_path}: root must be an object")
        entity_name = filename.removesuffix(".json")
        entity = wrapper.get(entity_name)
        order = wrapper.get("order")
        if not isinstance(entity, dict) or not isinstance(entity.get("id"), str):
            raise ValueError(f"{metadata_path}: missing {entity_name}.id")
        if not isinstance(order, int) or order < 0:
            raise ValueError(f"{metadata_path}: order must be a non-negative integer")
        if order in orders:
            raise ValueError(f"{source_dir / group}: duplicate order {order}")
        orders.add(order)
        if metadata_path.parent.name != entity["id"]:
            raise ValueError(
                f"{metadata_path}: parent directory must match id {entity['id']!r}"
            )
        if group in {"families", "machines"}:
            manufacturer_id = entity.get("manufacturer_id")
            if metadata_path.parent.parent.name != manufacturer_id:
                raise ValueError(
                    f"{metadata_path}: manufacturer directory must be {manufacturer_id!r}"
                )
        translations = load_locale_directory(metadata_path.parent / "locales", locales)
        bundles.append((order, entity["id"], wrapper, translations))
    return sorted(bundles, key=lambda item: (item[0], item[1]))


def assemble(source_dir: Path) -> tuple[dict[str, Any], dict[str, dict[str, str]]]:
    common_path = source_dir / "common" / "catalog.json"
    common = read_json(common_path)
    if not isinstance(common, dict) or common.get("source_schema") != SOURCE_SCHEMA:
        raise ValueError(f"{common_path}: source_schema must be {SOURCE_SCHEMA!r}")
    output_schema = common.get("output_schema")
    facets = common.get("filter_facets")
    if not isinstance(output_schema, str) or not isinstance(facets, list):
        raise ValueError(f"{common_path}: output_schema and filter_facets are required")

    translations = load_locale_directory(source_dir / "common" / "locales")
    if "en" not in translations:
        raise ValueError(f"{source_dir / 'common/locales'}: en.json is required")
    locale_names = set(translations)

    manufacturer_bundles = load_bundles(source_dir, "manufacturers", locale_names)
    family_bundles = load_bundles(source_dir, "families", locale_names)
    machine_bundles = load_bundles(source_dir, "machines", locale_names)
    research_path = common_path.with_name("research-facets.json")
    research = load_research_facets(research_path, facets) if research_path.exists() else None

    manufacturers = []
    families = []
    products = []
    ordered_platforms = []
    for _, _, wrapper, localized in manufacturer_bundles:
        manufacturers.append(wrapper["manufacturer"])
        merge_translations(translations, localized, source_dir / "manufacturers")
    for _, _, wrapper, localized in family_bundles:
        families.append(wrapper["family"])
        merge_translations(translations, localized, source_dir / "families")
    for _, _, wrapper, localized in machine_bundles:
        products.append(wrapper["machine"])
        platform = wrapper.get("platform")
        if platform is not None:
            platform_order = wrapper.get("platform_order")
            if not isinstance(platform, dict) or not isinstance(platform_order, int):
                raise ValueError("machine bundles with a platform require integer platform_order")
            ordered_platforms.append((platform_order, platform["id"], platform))
            if platform.get("id") != wrapper["machine"].get("platform_id"):
                raise ValueError(
                    f"machine {wrapper['machine']['id']!r}: platform id does not match platform_id"
                )
        elif wrapper["machine"].get("platform_id"):
            raise ValueError(
                f"machine {wrapper['machine']['id']!r}: platform metadata is missing"
            )
        if research is not None:
            apply_research_facets(wrapper["machine"], platform, research.get(wrapper["machine"]["id"]))
        merge_translations(translations, localized, source_dir / "machines")

    platforms = [item[2] for item in sorted(ordered_platforms, key=lambda item: (item[0], item[1]))]
    catalog = {
        "schema": output_schema,
        "filter_facets": facets,
        "manufacturers": manufacturers,
        "families": families,
        "platforms": platforms,
        "products": products,
    }
    return catalog, translations


def apply_research_facets(
    machine: dict[str, Any], platform: dict[str, Any] | None, review: dict[str, Any] | None,
) -> None:
    """Keep preservation metadata independent of the release status and visibility."""
    facets = machine.setdefault("facets", {})
    for facet_id in ("documentation", "firmware_custody"):
        facets[facet_id] = [review[facet_id] if review is not None else "unreviewed"]

    destinations = list(review["test_destinations"]) if review is not None else []
    if any(value.startswith("fpga_") for value in destinations) and "fpga" not in destinations:
        destinations.insert(0, "fpga")
    if (platform and machine.get("status") not in {"research", "not_bootable"}
            and (platform.get("emulator_machine_id") or platform.get("coremach_system_id"))):
        destinations.insert(0, "blumach")
    facets["test_destinations"] = destinations or ["none_known" if review is not None else "unreviewed"]


def load_research_facets(path: Path, facets: list[dict[str, Any]]) -> dict[str, Any]:
    # Entries can precede their machine bundles when catalogue PRs merge separately.
    # Check all rows, including the ones that will only be applied after those merges.
    fields = ("documentation", "firmware_custody", "test_destinations")
    allowed = {facet["id"]: {value["id"] for value in facet["values"]} for facet in facets}
    reviews = read_json(path)
    if not isinstance(reviews, dict):
        raise ValueError(f"{path}: expected an object keyed by product id")
    for product_id, row in reviews.items():
        if not catalog_audit.ENTITY_ID.fullmatch(product_id) or not isinstance(row, dict) or set(row) != set(fields):
            raise ValueError(f"{path}: invalid review for {product_id!r}")
        for field in fields:
            values = row[field] if field == "test_destinations" else [row[field]]
            if not isinstance(values, list) or any(
                not isinstance(value, str) or value not in allowed.get(field, ()) for value in values
            ) or len(set(values)) != len(values):
                raise ValueError(f"{path}: invalid {field} for {product_id!r}")
        if {"blumach", "none_known"} & set(row["test_destinations"]):
            raise ValueError(f"{path}: derived destinations are not allowed in {product_id!r}")
    return reviews


def implementation_documents(catalog: dict[str, Any]) -> list[str]:
    documents = set()
    for product in catalog.get("products", []):
        if not isinstance(product, dict):
            continue
        implementation = product.get("implementation")
        if isinstance(implementation, dict) and isinstance(implementation.get("document"), str):
            documents.add(implementation["document"])
    return sorted(documents)


def validate(
    catalog: dict[str, Any],
    translations: dict[str, dict[str, str]],
    documents_dir: Path | None = None,
) -> list[str]:
    errors: list[str] = []
    references = catalog_audit.validate_catalog(catalog, errors)
    catalog_audit.validate_urls(catalog, errors)
    catalog_audit.validate_translation_sets(translations, references, errors, "catalog source")
    if documents_dir is not None:
        for document in implementation_documents(catalog):
            if not (documents_dir / document).is_file():
                errors.append(f"implementation document not found: {document}")
    return errors


def render_qrc(locales: list[str], documents: list[str] | None = None) -> str:
    files = ["    <file alias=\"catalog.json\">catalog.json</file>"]
    files.extend(
        f"    <file alias=\"locales/{locale}.json\">locales/{locale}.json</file>"
        for locale in locales
    )
    files.extend(
        f"    <file alias=\"documents/{document}\">documents/{document}</file>"
        for document in documents or []
    )
    return "\n".join(
        ["<RCC>", '  <qresource prefix="/blumach/catalog">', *files, "  </qresource>", "</RCC>", ""]
    )


def build(source_dir: Path, output_dir: Path, documents_dir: Path) -> None:
    catalog, translations = assemble(source_dir)
    errors = validate(catalog, translations, documents_dir)
    if errors:
        raise ValueError("catalogue validation failed:\n  - " + "\n  - ".join(errors))
    write_json(output_dir / "catalog.json", catalog)
    for locale, localized in sorted(translations.items()):
        write_json(output_dir / "locales" / f"{locale}.json", localized)
    documents = implementation_documents(catalog)
    for document in documents:
        write_text_if_changed(
            output_dir / "documents" / document,
            (documents_dir / document).read_text(encoding="utf-8"),
        )
    write_text_if_changed(
        output_dir / "blumach_catalog.qrc",
        render_qrc(sorted(translations), documents),
    )


def main() -> int:
    repository = Path(__file__).resolve().parents[1]
    documents_dir = repository / "doc" / "machines"
    default_source = repository / "src" / "qt" / "catalog" / "source"
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    check_parser = subparsers.add_parser("check", help="assemble and validate without writing")
    check_parser.add_argument("--source", type=Path, default=default_source)

    build_parser = subparsers.add_parser("build", help="write generated runtime resources")
    build_parser.add_argument("--source", type=Path, default=default_source)
    build_parser.add_argument("--output", type=Path, required=True)

    args = parser.parse_args()
    try:
        if args.command == "check":
            catalog, translations = assemble(args.source.resolve())
            errors = validate(catalog, translations, documents_dir)
            if errors:
                raise ValueError("catalogue validation failed:\n  - " + "\n  - ".join(errors))
            print(
                f"BluMach catalogue source passed: {len(catalog['products'])} machines, "
                f"{len(translations)} locales"
            )
        elif args.command == "build":
            build(args.source.resolve(), args.output.resolve(), documents_dir)
            print(f"Generated BluMach catalogue resources in {args.output.resolve()}")
    except (KeyError, OSError, TypeError, ValueError) as exc:
        print(f"catalog_builder: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
