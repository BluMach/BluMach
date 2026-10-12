# SPDX-License-Identifier: GPL-2.0-or-later
"""Creation contracts for the classic M15 Plus; no firmware execution."""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import catalog_builder


class M15PlusCatalogTests(unittest.TestCase):
    def setUp(self):
        catalog, self.locales = catalog_builder.assemble(ROOT / 'src/qt/catalog/source')
        self.product = next(p for p in catalog['products'] if p['id'] == 'olivetti-m15-plus')
        self.creation = self.product['creation']
        self.field = next(f for f in self.creation['fields'] if f['id'] == 'commercial_model')

    def settings(self, choice):
        values = {(r['section'], k): v for r in self.creation['configuration'] for k, v in r['values'].items()}
        values.update({(r['section'], r['key']): r['value'] for r in choice.get('set', [])})
        return values

    def test_status_defaults_and_profiles(self):
        self.assertEqual('experimental', self.product['status'])
        self.assertEqual('experimental', self.creation['status'])
        self.assertEqual('dual-floppy', self.field['default'])
        self.assertEqual({'dual-floppy', 'hdu-20mb'}, {c['id'] for c in self.field['choices']})
        for choice in self.field['choices']:
            with self.subTest(profile=choice['id']):
                settings = self.settings(choice)
                self.assertEqual('8088', settings['Machine', 'cpu_family'])
                self.assertEqual(4772728, settings['Machine', 'cpu_speed'])
                self.assertEqual(512, settings['Machine', 'mem_size'])
                self.assertEqual('internal', settings['Video', 'gfxcard'])
                self.assertEqual('none', settings['Input devices', 'mouse_type'])
                self.assertEqual('35_2dd', settings['Floppy and CD-ROM drives', 'fdd_01_type'])

    def test_hdu_bay_and_generated_geometry(self):
        dual, = [c for c in self.field['choices'] if c['id'] == 'dual-floppy']
        settings = self.settings(dual)
        self.assertEqual('35_2dd', settings['Floppy and CD-ROM drives', 'fdd_02_type'])
        self.assertEqual('none', settings['Storage controllers', 'hdc_1'])
        self.assertNotIn(('Hard disks', 'hdd_01_fn'), settings)
        hdu, = [c for c in self.field['choices'] if c['id'] == 'hdu-20mb']
        settings = self.settings(hdu)
        self.assertEqual('none', settings['Floppy and CD-ROM drives', 'fdd_02_type'])
        self.assertEqual('internal', settings['Storage controllers', 'hdc_1'])
        self.assertEqual('17, 4, 615, 0, xta', settings['Hard disks', 'hdd_01_parameters'])
        generated, = hdu['generated_files']
        self.assertEqual(settings['Hard disks', 'hdd_01_fn'], generated['path'])
        self.assertEqual(615 * 4 * 17 * 512, generated['size'])

    def test_firmware_matches_native_loader(self):
        path, = self.product['firmware']['required_files']
        self.assertEqual('machines/olivetti_m15plus/Olivetti-M15-Plus-BIOS-1.10.bin', path)
        source = (ROOT / 'src/machine/m_xt.c').read_text(encoding='utf-8')
        self.assertRegex(source, re.escape('bios_load_linear("roms/' + path + '",') +
                         r'\s*0x000f8000,\s*32768,\s*0\)')

    def test_release_ownership_and_translations(self):
        source = (ROOT / 'src/machine/machine_table.c').read_text(encoding='utf-8')
        self.assertRegex(source, r'\.internal_name\s*=\s*"olivetti_m15plus",\s*'
                         r'\.blumach_release_managed\s*=\s*1,')
        for locale, texts in self.locales.items():
            with self.subTest(locale=locale):
                self.assertIn(self.product['warning_key'], texts)
                for choice in self.field['choices']:
                    self.assertIn(choice['label_key'], texts)


if __name__ == '__main__':
    unittest.main()
