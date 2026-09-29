"""PCS86 sound choices: bus, resource and creation contracts."""
import itertools
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class SoundChoicesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.creation = json.loads((ROOT / "src/qt/catalog/source/machines/olivetti/olivetti-pcs86/machine.json").read_text(encoding="utf-8"))["machine"]["creation"]
        cls.fields = {f["id"]: f for f in cls.creation["fields"]}
        cls.sound = cls.fields["sound_card"]
        cls.expected = set(["none","adlib","cms","sb","sb1.5","sb2.0"])

    def test_explicit_allowlist_and_default(self):
        self.assertEqual("none", self.sound["default"])
        values = set()
        for choice in self.sound["choices"]:
            if choice["status"] == "unavailable":
                self.assertEqual("awe32", choice["id"])
                self.assertNotIn("set", choice)
                continue
            self.assertEqual("validated" if choice["id"] == "none" else "experimental", choice["status"])
            self.assertEqual(1, len(choice["set"]))
            entry = choice["set"][0]
            self.assertEqual(("Sound", "sndcard"), (entry["section"], entry["key"]))
            values.add(entry["value"])
        self.assertEqual(self.expected, values)
        self.assertIn("product.olivetti-pcs86.creation.sound_note", self.creation["note_keys"])

    def test_enabled_cards_have_supported_bus_and_no_extra_rom(self):
        sources = "\n".join((ROOT / "src/sound" / f).read_text(encoding="utf-8")
                            for f in ("snd_adlib.c", "snd_cms.c", "snd_sb.c", "snd_gus.c", "snd_wss.c"))
        for name in self.expected - {"none"}:
            with self.subTest(name=name):
                match = re.search(r'\.internal_name\s*=\s*"' + re.escape(name) + r'",(?P<body>.*?)\n};', sources, re.S)
                self.assertIsNotNone(match, name)
                body = match.group("body")
                flags = re.search(r"\.flags\s*=\s*([^,]+),", body).group(1)
                self.assertNotRegex(flags, r"DEVICE_(MCA|PCI)")
                self.assertRegex(flags, r"DEVICE_ISA\b")
                self.assertNotIn("DEVICE_ISA16", flags)
                self.assertRegex(body, r"\.available\s*=\s*NULL")

    def test_all_creation_combinations_preserve_ram_storage_and_video(self):
        count = 0
        fields = list(self.fields.values())
        for choices in itertools.product(*([c for c in f["choices"] if c["status"] != "unavailable"] for f in fields)):
            settings = {(s["section"], k): v for s in self.creation["configuration"] for k, v in s["values"].items()}
            for choice in choices:
                for entry in choice.get("set", []):
                    settings[(entry["section"], entry["key"])] = entry["value"]
            self.assertEqual("internal", settings[("Video", "gfxcard")])
            self.assertIn(settings[("Sound", "sndcard")], self.expected)
            if ("Hard disks", "hdd_01_fn") in settings:
                self.assertEqual("none", settings[("Floppy and CD-ROM drives", "fdd_02_type")])
            memory = choices[fields.index(self.fields["memory_expansion"])]["set"][0]
            self.assertEqual(memory["value"], settings[(memory["section"], memory["key"])])
            count += 1
        self.assertEqual(144, count)


if __name__ == "__main__":
    unittest.main()
