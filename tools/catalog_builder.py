#!/usr/bin/env python3
"""Build deterministic BluMach catalogue resources from sharded sources."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
import sys
from typing import Any
from urllib.parse import urlsplit

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


VARIANT_GROUPS_FILE = "variant-groups.json"
VARIANT_GROUPS_SCHEMA = "blumach-variant-groups-v1"


def read_variant_group_list(path: Path) -> list[Any]:
    document = read_json(path)
    if (not isinstance(document, dict) or document.get("schema") != VARIANT_GROUPS_SCHEMA
            or not isinstance(document.get("groups"), list)):
        raise ValueError(f"{path}: invalid variant groups document")
    return document["groups"]


def load_variant_groups(
    source_dir: Path,
    catalog: dict[str, Any],
    translations: dict[str, dict[str, str]],
    *,
    desktop: bool = False,
) -> dict[str, Any]:
    common_path = source_dir / "common" / VARIANT_GROUPS_FILE
    paths = [common_path] if common_path.is_file() else []
    # Include application-only groups and their locale keys in Qt resources.
    desktop_dir = source_dir / "desktop"
    if desktop and desktop_dir.exists():
        paths.append(desktop_dir / VARIANT_GROUPS_FILE)
        desktop_locales = load_locale_directory(desktop_dir / "locales", set(translations))
        merge_translations(translations, desktop_locales, desktop_dir)
    groups = [(path, group) for path in paths for group in read_variant_group_list(path)]
    products = {item["id"]: item for item in catalog["products"]}
    visible_makers = {item["id"] for item in catalog["manufacturers"] if item.get("visible") is True}
    visible_families = {item["id"] for item in catalog["families"]
                        if item.get("visible") is True and item["manufacturer_id"] in visible_makers}
    used_ids = set(products) | {item["id"] for kind in ("manufacturers", "families") for item in catalog[kind]}
    grouped: set[str] = set()
    for path, group in groups:
        if not isinstance(group, dict):
            raise ValueError(f"{path}: each group must be an object")
        group_id = group.get("id")
        if not isinstance(group_id, str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]*", group_id) or group_id in used_ids:
            raise ValueError(f"{path}: invalid or duplicate group id {group_id!r}")
        used_ids.add(group_id)
        label = group.get("name")
        family = group.get("family_id")
        if not isinstance(label, str) or not label.strip() or not isinstance(family, str) or family not in visible_families:
            raise ValueError(f"{path}: {group_id}: name and visible family are required")
        for field in ("summary_key", "upgrades_key", "firmware_key"):
            if field != "summary_key" and field not in group:
                continue
            key = group.get(field)
            if not isinstance(key, str) or any(not pack.get(key, "").strip() for pack in translations.values()):
                raise ValueError(f"{path}: {group_id}: {field} requires text in all locales")
        url = group.get("source_url")
        if (not isinstance(url, str) or urlsplit(url).scheme != "https" or not urlsplit(url).hostname
                or catalog_audit.links_to_third_party_file(url)):
            raise ValueError(f"{path}: {group_id}: source_url must be an HTTPS source page")
        variants = group.get("variants")
        if not isinstance(variants, list) or len(variants) < 2:
            raise ValueError(f"{path}: {group_id}: at least two variants are required")
        for variant in variants:
            product = products.get(variant) if isinstance(variant, str) else None
            if product is None or product.get("visible") is not True or product.get("family_id") != family:
                raise ValueError(f"{path}: {group_id}: variant {variant!r} must be visible and belong to {family}")
            if variant in grouped:
                raise ValueError(f"{path}: {group_id}: variant {variant!r} occurs in more than one position")
            grouped.add(variant)
    return {"schema": VARIANT_GROUPS_SCHEMA, "groups": [group for _, group in groups]}


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
    files = [f'    <file alias="{name}">{name}</file>' for name in ("catalog.json", VARIANT_GROUPS_FILE)]
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
    variant_groups = load_variant_groups(source_dir, catalog, translations, desktop=True)
    write_json(output_dir / "catalog.json", catalog)
    write_json(output_dir / VARIANT_GROUPS_FILE, variant_groups)
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
            load_variant_groups(args.source.resolve(), catalog, translations, desktop=True)
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
