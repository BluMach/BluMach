#!/usr/bin/env python3
"""Export the public part of the BluMach catalogue for the website.

The archive holds what the application's Collection shows about each machine
(history, technical sheet, status, sources, images with their credits) in
every catalogue language. It leaves out what only the application needs to
create a machine: creation templates, emulator machine identifiers and
CoreMach system identifiers. Entries the application hides (without
"visible": true, including development-only ones) are left out entirely,
together with their texts. Every exported entry keeps its "visible": true so
the website can refuse anything else.
"""

from __future__ import annotations

import argparse
import copy
import io
import json
from pathlib import Path
import sys
from typing import Any
import xml.etree.ElementTree as ET
import zipfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

import catalog_builder  # noqa: E402


SCHEMA = "blumach-catalog-web-v1"
REPOSITORY = "https://github.com/BluMach/BluMach"
RESOURCE_PREFIX = ":/blumach/catalog/images/"
# Product fields only the application uses to create or filter machines.
PRIVATE_PRODUCT_FIELDS = ("creation", "filter_profiles", "platform_id", "development")
# Fixed timestamp so the same catalogue always gives the same archive bytes.
ZIP_TIMESTAMP = (2000, 1, 1, 0, 0, 0)
# The archive's HTTPS equivalents could not be verified. Keep the translated
# issue/page citations in the public catalogue and the original links in BluMach.
LEGACY_SCHNEIDER_PRESS = "http://www.cc-computerarchiv.de/CC-Archiv/bc-alt/gb-schn/"


def image_files(resource_file: Path) -> dict[str, Path]:
    """Map each catalogue image resource to its file through the Qt resource list."""
    root = ET.parse(resource_file).getroot()
    files = {}
    for group in root.findall("qresource"):
        if group.get("prefix") != "/blumach/catalog":
            continue
        for item in group.findall("file"):
            alias = item.get("alias", item.text or "")
            if alias.startswith("images/") and item.text:
                files[alias[len("images/"):]] = resource_file.parent / item.text
    return files


def visible(entity: dict[str, Any]) -> bool:
    """Same rule as the application's release catalogue: only an explicit "visible": true."""
    return entity.get("visible") is True


def hidden_text(key: str, hidden: dict[str, set[str]]) -> bool:
    kind, _, rest = key.partition(".")
    return any(rest.startswith(f"{entity_id}.") for entity_id in hidden.get(kind, ()))


def public_product(
    product: dict[str, Any],
    platforms: dict[str, dict[str, Any]],
    tag: str,
    images: set[str],
) -> dict[str, Any]:
    result = copy.deepcopy(product)
    for field in PRIVATE_PRODUCT_FIELDS:
        result.pop(field, None)
    platform = platforms.get(product.get("platform_id") or "")
    if platform is not None:
        result["engine"] = platform.get("engine", catalog_builder.catalog_audit.DEFAULT_ENGINE)
        if platform.get("architecture"):
            result["architecture"] = platform["architecture"]
    media = result.get("media")
    if isinstance(media, dict):
        resource = media.get("resource", "")
        if not resource.startswith(RESOURCE_PREFIX):
            raise ValueError(f"product {product['id']!r}: unexpected media resource {resource!r}")
        name = resource[len(RESOURCE_PREFIX):]
        images.add(name)
        media["resource"] = f"images/{name}"
    for section in result.get("technical", []):
        for entry in section["entries"]:
            if entry.get("url", "").startswith(LEGACY_SCHNEIDER_PRESS):
                del entry["url"]
    implementation = result.get("implementation")
    if isinstance(implementation, dict):
        implementation["url"] = f"{REPOSITORY}/blob/{tag}/doc/machines/{implementation['document']}"
    return result


def export(
    source_dir: Path,
    resource_file: Path,
    version: str,
    commit: str,
    *,
    include_variant_groups: bool = False,
) -> dict[str, bytes]:
    catalog, translations = catalog_builder.assemble(source_dir)
    errors = catalog_builder.validate(catalog, translations)
    if errors:
        raise ValueError("catalogue does not validate:\n" + "\n".join(errors))

    tag = f"v{version}"
    platforms = {platform["id"]: platform for platform in catalog["platforms"]}
    manufacturers = [m for m in catalog["manufacturers"] if visible(m)]
    manufacturer_ids = {m["id"] for m in manufacturers}
    families = [f for f in catalog["families"]
                if visible(f) and f.get("manufacturer_id") in manufacturer_ids]
    family_ids = {f["id"] for f in families}
    hidden = {
        "manufacturer": {m["id"] for m in catalog["manufacturers"]} - manufacturer_ids,
        "family": {f["id"] for f in catalog["families"]} - family_ids,
    }
    images: set[str] = set()
    products = [
        public_product(product, platforms, tag, images)
        for product in catalog["products"]
        if visible(product) and product.get("family_id") in family_ids
    ]

    hidden["product"] = {p["id"] for p in catalog["products"]} - {p["id"] for p in products}
    for product in products:
        relationships = product.get("relationships")
        if isinstance(relationships, dict):
            for kind in list(relationships):
                kept = [item for item in relationships[kind] if item not in hidden["product"]]
                if kept:
                    relationships[kind] = kept
                else:
                    del relationships[kind]
            if not relationships:
                del product["relationships"]

    files: dict[str, bytes] = {}
    variant_groups = catalog_builder.load_variant_groups(source_dir, catalog, translations)
    if include_variant_groups and variant_groups["groups"]:
        files[catalog_builder.VARIANT_GROUPS_FILE] = encode(variant_groups)
    image_paths = image_files(resource_file)
    for name in sorted(images):
        path = image_paths.get(name)
        if path is None or not path.is_file():
            raise ValueError(f"image resource not found: {name} ({path})")
        files[f"images/{name}"] = path.read_bytes()
    for locale, values in sorted(translations.items()):
        files[f"locales/{locale}.json"] = encode(
            {key: value for key, value in values.items() if not hidden_text(key, hidden)})
    files["catalog.json"] = encode({
        "filter_facets": catalog["filter_facets"],
        "manufacturers": manufacturers,
        "families": families,
        "products": products,
    })
    files["manifest.json"] = encode({
        "schema": SCHEMA,
        "version": version,
        "tag": tag,
        "commit": commit,
        "repository": REPOSITORY,
        "locales": sorted(translations),
        "products": len(products),
    })
    return files


def encode(value: Any) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def write_zip(files: dict[str, bytes], output: Path) -> None:
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(files):
            info = zipfile.ZipInfo(name, ZIP_TIMESTAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, files[name])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(buffer.getvalue())


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="BluMach version, without the leading v")
    parser.add_argument("--commit", required=True, help="BluMach commit the catalogue comes from")
    parser.add_argument("--output", type=Path, required=True, help="ZIP file to write")
    parser.add_argument("--source", type=Path, default=root / "src/qt/catalog/source")
    parser.add_argument("--resources", type=Path, default=root / "src/qt_resources.qrc")
    parser.add_argument("--include-variant-groups", action="store_true",
                        help="include presentation data for an importer that supports variant groups")
    arguments = parser.parse_args()
    try:
        files = export(arguments.source, arguments.resources, arguments.version, arguments.commit,
                       include_variant_groups=arguments.include_variant_groups)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 1
    write_zip(files, arguments.output)
    manifest = json.loads(files["manifest.json"])
    print(f"Exported {manifest['products']} machines in {len(manifest['locales'])} languages "
          f"to {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
