#!/usr/bin/env python3
"""PCS 286 creation contract: motherboard video and optional ISA upgrades."""

import itertools
import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]


class Pcs286CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        bundle = ROOT / "src/qt/catalog/source/machines/olivetti/olivetti-pcs286"
        cls.creation = json.loads((bundle / "machine.json").read_text(encoding="utf-8"))["machine"]["creation"]
        cls.fields = {field["id"]: field for field in cls.creation["fields"]}

    def test_internal_video_is_available_in_native_configure(self):
        table = (ROOT / "src/machine/machine_table.c").read_text(encoding="utf-8")
        record = table.split('.internal_name     = "olivetti_pcs286",', 1)[1].split('.aliases', 1)[0]
        self.assertRegex(record, r"\.flags\s*=\s*MACHINE_VIDEO\s*,")
        self.assertRegex(record, r"\.vid_device\s*=\s*&paradise_pvga1a_pcs286_device\s*,")
        for key, value in (("min", 1024), ("max", 4096), ("step", 1024)):
            self.assertRegex(record, rf"\.{key}\s*=\s*{value}\b")

    def test_every_creation_combination_keeps_motherboard_video(self):
        fields = list(self.fields.values())
        count = 0
        for choices in itertools.product(*(field["choices"] for field in fields)):
            with self.subTest(choices=[choice["id"] for choice in choices]):
                settings = {
                    (section["section"], key): value
                    for section in self.creation["configuration"]
                    for key, value in section["values"].items()
                }
                for choice in choices:
                    for entry in choice.get("set", []):
                        settings[(entry["section"], entry["key"])] = entry["value"]
                self.assertEqual("internal", settings[("Video", "gfxcard")])
                self.assertEqual("v142", settings[("Olivetti PCS 286", "bios")])
                self.assertIn(settings[("Machine", "mem_size")], (1024, 2048, 4096))
                self.assertIn(settings[("Sound", "sndcard")], ("none", "adlib", "sb1.5", "sb2.0", "sbprov1", "sbprov2"))
                if ("Hard disks", "hdd_01_fn") in settings:
                    self.assertEqual("none", settings[("Floppy and CD-ROM drives", "fdd_02_type")])
                count += 1
        self.assertEqual(54, count)

    def test_memory_and_sound_choices_have_conservative_defaults(self):
        self.assertEqual("1mb", self.fields["memory"]["default"])
        self.assertEqual({"1mb", "2mb", "4mb"}, {c["id"] for c in self.fields["memory"]["choices"]})
        self.assertEqual("none", self.fields["sound_card"]["default"])
        for choice in self.fields["sound_card"]["choices"]:
            if choice["id"] != "none":
                self.assertEqual("experimental", choice["status"])

    def test_sound_choices_resolve_to_existing_isa_devices(self):
        sources = "\n".join((ROOT / path).read_text(encoding="utf-8") for path in
                            ("src/sound/snd_adlib.c", "src/sound/snd_sb.c"))
        for name in ("adlib", "sb1.5", "sb2.0", "sbprov1", "sbprov2"):
            with self.subTest(name=name):
                match = re.search(rf'\.internal_name\s*=\s*"{re.escape(name)}",\s*\.flags\s*=\s*([^,]+),', sources)
                self.assertIsNotNone(match, name)
                flags = {flag.strip() for flag in match.group(1).split("|")}
                self.assertIn("DEVICE_ISA", flags)
                self.assertNotIn("DEVICE_MCA", flags)

    def test_sound_allowlist_does_not_assume_isa_means_286_software(self):
        values = {setting["value"] for choice in self.fields["sound_card"]["choices"]
                  for setting in choice["set"] if setting["key"] == "sndcard"}
        self.assertEqual({"none", "adlib", "sb1.5", "sb2.0", "sbprov1", "sbprov2"}, values)
        self.assertIn("product.olivetti-pcs286.creation.sound_286_note", self.creation["note_keys"])


if __name__ == "__main__":
    unittest.main()
