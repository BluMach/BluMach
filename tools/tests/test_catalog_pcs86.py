#!/usr/bin/env python3

import json
from pathlib import Path
import unittest


class Pcs86CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        repository = Path(__file__).resolve().parents[2]
        bundle = repository / "src/qt/catalog/source/machines/olivetti/olivetti-pcs86/machine.json"
        cls.machine = json.loads(bundle.read_text(encoding="utf-8"))["machine"]
        cls.fields = {
            field["id"]: field for field in cls.machine["creation"]["fields"]
        }

    @staticmethod
    def settings(choice: dict) -> dict[tuple[str, str], object]:
        return {
            (setting["section"], setting["key"]): setting["value"]
            for setting in choice.get("set", [])
        }

    def test_commercial_storage_profiles_match_the_brochure(self) -> None:
        choices = {
            choice["id"]: choice
            for choice in self.fields["commercial_configuration"]["choices"]
        }

        self.assertEqual("one-720", self.fields["commercial_configuration"]["default"])
        self.assertEqual(
            {"one-720", "two-720", "one-720-hdd20"},
            {choice_id for choice_id, choice in choices.items() if choice["status"] == "documented"},
        )
        hdd = self.settings(choices["one-720-hdd20"])
        self.assertEqual("35_2dd", hdd[("Floppy and CD-ROM drives", "fdd_01_type")])
        self.assertEqual("none", hdd[("Floppy and CD-ROM drives", "fdd_02_type")])
        self.assertEqual("17, 4, 615, 0, xta", hdd[("Hard disks", "hdd_01_parameters")])
        self.assertEqual(21411840, choices["one-720-hdd20"]["generated_files"][0]["size"])

    def test_144mb_drives_are_separate_upgrades(self) -> None:
        choices = {
            choice["id"]: choice
            for choice in self.fields["commercial_configuration"]["choices"]
        }

        one = self.settings(choices["one-144"])
        one_hdd = self.settings(choices["one-144-hdd20"])
        two = self.settings(choices["two-144"])
        self.assertEqual("experimental", choices["one-144"]["status"])
        self.assertEqual("35_2hd", one[("Floppy and CD-ROM drives", "fdd_01_type")])
        self.assertEqual("none", one[("Floppy and CD-ROM drives", "fdd_02_type")])
        self.assertEqual("35_2hd", one_hdd[("Floppy and CD-ROM drives", "fdd_01_type")])
        self.assertEqual("none", one_hdd[("Floppy and CD-ROM drives", "fdd_02_type")])
        self.assertEqual("17, 4, 615, 0, xta", one_hdd[("Hard disks", "hdd_01_parameters")])
        self.assertEqual(21411840, choices["one-144-hdd20"]["generated_files"][0]["size"])
        self.assertEqual("35_2hd", two[("Floppy and CD-ROM drives", "fdd_01_type")])
        self.assertEqual("35_2hd", two[("Floppy and CD-ROM drives", "fdd_02_type")])

    def test_memory_and_optional_expansions_use_supported_values(self) -> None:
        memory = {
            choice["id"]: self.settings(choice)
            for choice in self.fields["memory_expansion"]["choices"]
        }
        fpu = {
            choice["id"]: self.settings(choice)
            for choice in self.fields["coprocessor"]["choices"]
        }
        sound = {
            choice["id"]: self.settings(choice)
            for choice in self.fields["sound_card"]["choices"]
        }
        configuration = {
            section["section"]: section["values"]
            for section in self.machine["creation"]["configuration"]
        }

        self.assertEqual(384, memory["two-256"][("Olivetti PCS86", "ems_size")])
        self.assertEqual(1920, memory["two-1024"][("Olivetti PCS86", "ems_size")])
        self.assertNotIn("ems_size", configuration["Machine"])
        self.assertEqual(384, configuration["Olivetti PCS86"]["ems_size"])
        self.assertEqual(-1, configuration["Olivetti PCS86"]["fdd0_jumpers"])
        self.assertEqual("8087", fpu["8087"][("Machine", "fpu_type")])
        self.assertEqual("adlib", sound["adlib"][("Sound", "sndcard")])


if __name__ == "__main__":
    unittest.main()
