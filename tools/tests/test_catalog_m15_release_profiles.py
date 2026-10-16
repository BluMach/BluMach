# SPDX-License-Identifier: GPL-2.0-or-later
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import catalog_builder


class M15ReleaseProfileTests(unittest.TestCase):
    def setUp(self):
        self.catalog, self.locales = catalog_builder.assemble(ROOT / 'src/qt/catalog/source')
        self.products = {p['id']: p for p in self.catalog['products']}

    def test_m_range_keeps_selected_release_models_only(self):
        family = next(f for f in self.catalog['families'] if f['id'] == 'olivetti-m')
        self.assertTrue(family['visible'])
        visible = {p['id'] for p in self.catalog['products'] if p['family_id']=='olivetti-m' and p.get('visible')}
        self.assertEqual({'olivetti-m15', 'olivetti-m15-plus', 'olivetti-m28', 'olivetti-m380',
                          'olivetti-m250', 'olivetti-m250e', 'olivetti-m280'}, visible)
        for model in ('olivetti-m15', 'olivetti-m15-plus'):
            self.assertEqual('experimental', self.products[model]['status'])

    def test_profiles_have_no_sound_cards_or_standalone_mpu(self):
        for model in ('olivetti-m15', 'olivetti-m15-plus'):
            p = self.products[model]
            config = {r['section']: r['values'] for r in p['creation']['configuration']}
            self.assertEqual({'sndcard':'none','sndcard2':'none','sndcard3':'none','sndcard4':'none','mpu401_standalone':0}, config['Sound'])
            fields = {f['id'] for f in p['creation']['fields']}
            self.assertNotIn('sound_card', fields)
            key = 'product.'+model+'.technical.expansion_options'
            for locale, texts in self.locales.items():
                with self.subTest(model=model,locale=locale):
                    self.assertTrue(texts[key])

    def test_native_policy_is_machine_owned_and_firmware_is_identified(self):
        table = (ROOT/'src/machine/machine_table.c').read_text(encoding='utf-8')
        for model,native in (('olivetti-m15','olivetti_m15'),('olivetti-m15-plus','olivetti_m15plus')):
            self.assertRegex(table, rf'\.internal_name\s*=\s*"{native}",\s*\.blumach_release_managed\s*=\s*1,\s*\.no_sound_expansion\s*=\s*1,')
            self.assertEqual(1,len(self.products[model]['firmware']['required_files']))

    def test_base_memory_and_lcd_limit_remain_explicit(self):
        p = self.products['olivetti-m15']
        choices = next(f for f in p['creation']['fields'] if f['id']=='memory')['choices']
        self.assertEqual({256,512},{c['set'][0]['value'] for c in choices})
        self.assertIn('four',p['hardware']['video'])
        self.assertIn('six',p['hardware']['video'])


if __name__=='__main__':
    unittest.main()
