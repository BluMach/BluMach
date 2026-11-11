# BluMach modifications: rtzor, Project BluMach, 2026.
"""Exercise MM58174 registers and the production NVR load/save implementation."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class OlivettiM24RtcTests(unittest.TestCase):
    def test_counter_and_persistence_contracts(self):
        compiler = shutil.which("gcc")
        if not compiler:
            local = Path("C:/msys64/ucrt64/bin/gcc.exe")
            compiler = str(local) if local.is_file() else None
        if not compiler:
            self.skipTest("GCC is required for the production RTC tests")
        environment = os.environ.copy()
        environment["PATH"] = str(Path(compiler).resolve().parent) + os.pathsep + environment.get("PATH", "")
        output_root = Path(os.environ.get("BLUMACH_NATIVE_TEST_DIR", tempfile.gettempdir())).resolve()
        output_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="blumach-m24-rtc-", dir=output_root) as directory:
            output = Path(directory)
            for configuration in ("Debug", "Release"):
                with self.subTest(configuration=configuration):
                    binary = output / (configuration + (".exe" if os.name == "nt" else ""))
                    command = [compiler, "-std=gnu11", "-O2", "-fwhole-program", "-Werror=sign-compare",
                               "-I", str(ROOT / "src/include"), "-I", str(ROOT / "src/cpu"),
                               str(ROOT / "tools/tests/m24-rtc.c"), "-o", str(binary)]
                    if configuration == "Debug":
                        command.append("-DDEBUG")
                    result = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True, text=True, timeout=120)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    fixture = output / (configuration + "-profile")
                    fixture.mkdir()
                    result = subprocess.run([str(binary), str(fixture)], cwd=ROOT, env=environment,
                                            capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("MM58174: PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
