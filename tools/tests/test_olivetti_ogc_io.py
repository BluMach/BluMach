# BluMach modifications: rtzor, Project BluMach, 2026.
"""Exercise OGC/CGA production I/O handlers without a VM or firmware."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class OlivettiOgcIoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc")
        if not compiler:
            local_gcc = Path("C:/msys64/ucrt64/bin/gcc.exe")
            compiler = str(local_gcc) if local_gcc.is_file() else None
        if not compiler:
            raise unittest.SkipTest("GCC is required for the production C I/O tests")

        cls.directory = tempfile.TemporaryDirectory(prefix="blumach-ogc-io-")
        cls.addClassCleanup(cls.directory.cleanup)
        cls.binary = Path(cls.directory.name) / (
            "olivetti-ogc-io.exe" if os.name == "nt" else "olivetti-ogc-io"
        )
        cls.environment = os.environ.copy()
        cls.environment["PATH"] = (
            str(Path(compiler).resolve().parent)
            + os.pathsep
            + cls.environment.get("PATH", "")
        )
        # Whole-program LTO drops unrelated renderer/device entry points on
        # both ELF and PE linkers. The tested register and timing code is the
        # unmodified production translation units, not a test reimplementation.
        command = [
            compiler, "-std=gnu11", "-O2", "-flto", "-fwhole-program",
            "-I", str(ROOT / "src/include"),
            "-I", str(ROOT / "src/cpu"),
            str(ROOT / "tools/tests/olivetti-ogc-io.c"),
            str(ROOT / "src/video/vid_cga.c"),
            str(ROOT / "src/video/vid_cga_olivetti.c"),
            "-o", str(cls.binary),
        ]
        result = subprocess.run(
            command, cwd=ROOT, env=cls.environment,
            capture_output=True, text=True, timeout=120,
        )
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def run_case(self, name):
        result = subprocess.run(
            [str(self.binary), name], cwd=ROOT, env=self.environment,
            capture_output=True, text=True, timeout=20,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_go380_crtc_aliases_and_full_cursor_low_byte(self):
        self.run_case("aliases")

    def test_other_profiles_keep_existing_crtc_decoding(self):
        self.run_case("other-profiles")

    def test_extended_ports_and_control_addresses(self):
        self.run_case("upper-ports")

    def test_go380_m19_and_generic_timing_paths(self):
        self.run_case("timings")


if __name__ == "__main__":
    unittest.main()
