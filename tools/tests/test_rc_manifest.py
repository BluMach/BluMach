from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import rc_manifest  # noqa: E402


def git(directory: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", "-c", "user.name=Test", "-c", "user.email=test@example.invalid", *arguments],
        cwd=directory,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def commit(directory: Path, name: str) -> str:
    (directory / name).write_text(name, encoding="utf-8")
    git(directory, "add", name)
    git(directory, "commit", "-q", "-m", name)
    return git(directory, "rev-parse", "HEAD")


def source_with_submodule(root: Path, checkout: bool) -> tuple[Path, str]:
    """A source repository pinning external/coremach, optionally checked out."""
    source = root / "source"
    source.mkdir()
    git(source, "init", "-q")
    engine = source / "external" / "coremach"
    engine.mkdir(parents=True)
    git(engine, "init", "-q")
    pinned = commit(engine, "engine")
    git(source, "update-index", "--add", "--cacheinfo", f"160000,{pinned},external/coremach")
    git(source, "commit", "-q", "-m", "pin")
    if not checkout:
        # Git marks its objects read-only, which Windows refuses to delete.
        for path in engine.rglob("*"):
            path.chmod(0o700)
        shutil.rmtree(engine)
        engine.mkdir()
    return source, pinned


class RcManifestTests(unittest.TestCase):
    def test_records_the_pinned_coremach_commit(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source, pinned = source_with_submodule(Path(directory), checkout=True)
            self.assertEqual(rc_manifest.submodule_commit(source, "external/coremach"), pinned)

    def test_rejects_a_missing_coremach_checkout(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source, _ = source_with_submodule(Path(directory), checkout=False)
            with self.assertRaisesRegex(rc_manifest.ManifestError, "not checked out"):
                rc_manifest.submodule_commit(source, "external/coremach")

    def test_rejects_a_coremach_checkout_at_another_commit(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source, _ = source_with_submodule(Path(directory), checkout=True)
            commit(source / "external" / "coremach", "newer")
            with self.assertRaisesRegex(rc_manifest.ManifestError, "but the source pins"):
                rc_manifest.submodule_commit(source, "external/coremach")

    def test_rejects_firmware_and_media_paths(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = [root / "roms" / "machine.bin", root / "disk.img"]
            for path in files:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"restricted")
            with self.assertRaisesRegex(rc_manifest.ManifestError, "firmware or guest media"):
                rc_manifest.reject_restricted_assets(root, files)

    def test_accepts_normal_runtime_files(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = [root / "BluMach.exe", root / "platforms" / "qwindows.dll"]
            for path in files:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"runtime")
            rc_manifest.reject_restricted_assets(root, files)

    def test_sha256_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "file"
            path.write_bytes(b"BluMach")
            self.assertEqual(
                rc_manifest.sha256(path),
                "82666f6b9585157591399c5b923ffcbc1478184220cb4cab1d82be568601384b",
            )


if __name__ == "__main__":
    unittest.main()
