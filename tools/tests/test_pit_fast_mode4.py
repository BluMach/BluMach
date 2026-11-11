# BluMach modifications: rtzor, Project BluMach, 2026.
"""Build and exercise the complete FAST PIT with the production timer scheduler.

GCC is required. BLUMACH_NATIVE_TEST_DIR can select the build-output directory.
No firmware or VM is needed; both Debug and Release preprocessing are checked.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class FastPitMode4Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("gcc")
        if not compiler:
            local_gcc = Path("C:/msys64/ucrt64/bin/gcc.exe")
            compiler = str(local_gcc) if local_gcc.is_file() else None
        if not compiler:
            raise unittest.SkipTest("GCC is required for the native FAST PIT tests")
        cls.environment = os.environ.copy()
        cls.environment["PATH"] = (
            str(Path(compiler).resolve().parent)
            + os.pathsep + cls.environment.get("PATH", "")
        )
        output_root = Path(os.environ.get("BLUMACH_NATIVE_TEST_DIR", tempfile.gettempdir())).resolve()
        output_root.mkdir(parents=True, exist_ok=True)
        cls.directory = tempfile.TemporaryDirectory(prefix="blumach-pit-", dir=output_root)
        cls.addClassCleanup(cls.directory.cleanup)
        output = Path(cls.directory.name).resolve()
        assert output.is_relative_to(output_root)
        cls.binaries = {}
        for configuration in ("Debug", "Release"):
            binary = output / (configuration + (".exe" if os.name == "nt" else ""))
            command = [
                compiler, "-std=gnu11", "-O2", "-DCMAKE",
                "-I", str(ROOT / "src/include"), "-I", str(ROOT / "src/cpu"),
                str(ROOT / "tools/tests/pit-fast-mode4.c"), "-o", str(binary),
            ]
            if configuration == "Debug":
                command.append("-DDEBUG")
            else:
                command.append("-DNDEBUG")
            result = subprocess.run(command, cwd=ROOT, env=cls.environment,
                                    capture_output=True, text=True, timeout=120)
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)
            cls.binaries[configuration] = binary

    def run_group(self, name):
        for configuration, binary in self.binaries.items():
            with self.subTest(configuration=configuration):
                result = subprocess.run([str(binary), name], cwd=ROOT,
                                        env=self.environment, capture_output=True,
                                        text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("PASS", result.stdout)

    def test_counts_gate_retrigger_and_single_clock_strobe(self):
        self.run_group("counts")

    def test_fractional_period_changes_rebases_and_clock_phase(self):
        self.run_group("timebase")

    def test_internal_external_clock_transitions(self):
        self.run_group("external")

    def test_existing_other_mode_sequences(self):
        self.run_group("other-modes")

    def test_counter_and_phase_timer_ownership(self):
        self.run_group("ownership")


if __name__ == "__main__":
    unittest.main()
