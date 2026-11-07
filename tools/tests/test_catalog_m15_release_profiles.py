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

    def test_m_range_visibility_matches_profiles_and_historical_entries(self):
        family = next(f for f in self.catalog['families'] if f['id'] == 'olivetti-m')
        self.assertTrue(family['visible'])
        visible = {p['id'] for p in self.catalog['products'] if p['family_id']=='olivetti-m' and p.get('visible')}
        self.assertEqual({
                "olivetti-cp486", "olivetti-echos-20", "olivetti-echos-20-color",
                "olivetti-echos-24", "olivetti-echos-24c", "olivetti-echos-26c",
                "olivetti-echos-42", "olivetti-echos-43-color", "olivetti-echos-44-color",
                "olivetti-echos-46-color", "olivetti-echos-48-color-plus", "olivetti-echos-p100c",
                "olivetti-echos-p100d", "olivetti-echos-p100e", "olivetti-echos-p120d",
                "olivetti-echos-p120e", "olivetti-echos-p120s", "olivetti-echos-p133c",
                "olivetti-echos-p133s", "olivetti-echos-p75", "olivetti-echos-p90",
                "olivetti-echos-p90s", "olivetti-echos-pro-133-s", "olivetti-echos-pro-150s",
                "olivetti-echos-pro-166xm", "olivetti-envision-400", "olivetti-envision-p75",
                "olivetti-laptop-d33", "olivetti-laptop-d33-c", "olivetti-laptop-lx4-c",
                "olivetti-laptop-s20", "olivetti-lsx-5010", "olivetti-lsx-5015",
                "olivetti-lsx-5020", "olivetti-lsx-5025", "olivetti-lsx-5025e", "olivetti-lsx-5030",
                "olivetti-lsx-5050", "olivetti-m10", "olivetti-m111", "olivetti-m15",
                "olivetti-m15-plus", "olivetti-m18p", "olivetti-m19", "olivetti-m20",
                "olivetti-m20-d", "olivetti-m20-st", "olivetti-m200", "olivetti-m21",
                "olivetti-m211", "olivetti-m211v", "olivetti-m22", "olivetti-m24",
                "olivetti-m24-new-p100", "olivetti-m24-new-p133", "olivetti-m24-new-p75",
                "olivetti-m240", "olivetti-m24sp", "olivetti-m250", "olivetti-m250e",
                "olivetti-m28", "olivetti-m28-20", "olivetti-m28-20c", "olivetti-m28-40",
                "olivetti-m28-40c", "olivetti-m28-sp", "olivetti-m280", "olivetti-m290",
                "olivetti-m290-20", "olivetti-m290-25", "olivetti-m290-30", "olivetti-m290s",
                "olivetti-m290sp", "olivetti-m30", "olivetti-m300-01", "olivetti-m300-04",
                "olivetti-m300-05", "olivetti-m300-10", "olivetti-m300-25", "olivetti-m300-28",
                "olivetti-m300-if378", "olivetti-m30002", "olivetti-m30002f", "olivetti-m30008",
                "olivetti-m30015", "olivetti-m30030", "olivetti-m30030p", "olivetti-m316",
                "olivetti-m34", "olivetti-m380", "olivetti-m380-40", "olivetti-m380-c",
                "olivetti-m380-c-new", "olivetti-m380-xp1", "olivetti-m380-xp3",
                "olivetti-m380-xp4", "olivetti-m380-xp5", "olivetti-m380-xp7", "olivetti-m380-xp9",
                "olivetti-m380t", "olivetti-m386-25", "olivetti-m4-100", "olivetti-m4-100-s",
                "olivetti-m4-133", "olivetti-m4-133-s", "olivetti-m4-34", "olivetti-m4-40",
                "olivetti-m4-422", "olivetti-m4-422s", "olivetti-m4-424", "olivetti-m4-424s",
                "olivetti-m4-432", "olivetti-m4-432s", "olivetti-m4-434", "olivetti-m4-434s",
                "olivetti-m4-452", "olivetti-m4-452s", "olivetti-m4-454", "olivetti-m4-454s",
                "olivetti-m4-46", "olivetti-m4-464", "olivetti-m4-464s", "olivetti-m4-484",
                "olivetti-m4-484s", "olivetti-m4-554", "olivetti-m4-554-s", "olivetti-m4-564",
                "olivetti-m4-564-s", "olivetti-m4-584", "olivetti-m4-584-s", "olivetti-m4-60",
                "olivetti-m4-62", "olivetti-m4-64", "olivetti-m4-65", "olivetti-m4-66",
                "olivetti-m4-75", "olivetti-m4-75-s", "olivetti-m4-82", "olivetti-m4-90",
                "olivetti-m4-90-s", "olivetti-m4-p100", "olivetti-m4-p100-s", "olivetti-m4-p133",
                "olivetti-m4-p133-s", "olivetti-m4-p75", "olivetti-m4-p75-s", "olivetti-m4-p90-s",
                "olivetti-m4-pxx-i", "olivetti-m40", "olivetti-m400-10", "olivetti-m400-40",
                "olivetti-m400-60", "olivetti-m44", "olivetti-m480-10", "olivetti-m480-20",
                "olivetti-m480-30", "olivetti-m480-40", "olivetti-m480-60", "olivetti-m486-esdi",
                "olivetti-m486-scsi", "olivetti-m54", "olivetti-m6-400", "olivetti-m6-420",
                "olivetti-m6-440", "olivetti-m6-450", "olivetti-m6-460", "olivetti-m6-520",
                "olivetti-m6-540", "olivetti-m6-560", "olivetti-m6-620", "olivetti-m6-6200",
                "olivetti-m6-640", "olivetti-m6-640-dp", "olivetti-m6-650", "olivetti-m6-650-dp",
                "olivetti-m6-670", "olivetti-m6-680", "olivetti-m6-680-dp", "olivetti-m6-690",
                "olivetti-m6-690-dp", "olivetti-m6-750", "olivetti-m6-750-s", "olivetti-m6-760",
                "olivetti-m6-760-s", "olivetti-m6-770", "olivetti-m6-770-s", "olivetti-m6-850",
                "olivetti-m6-860", "olivetti-m6-880", "olivetti-m6-950", "olivetti-m60",
                "olivetti-m60-2", "olivetti-m60-3", "olivetti-m64", "olivetti-m70",
                "olivetti-m70-2", "olivetti-m70-3", "olivetti-m700-10", "olivetti-modulo-p100-l",
                "olivetti-modulo-p120-l", "olivetti-modulo-p133-l", "olivetti-modulo-pro-180",
                "olivetti-netstrada-1000", "olivetti-netstrada-1400", "olivetti-netstrada-2600",
                "olivetti-netstrada-3000", "olivetti-netstrada-3400",
                "olivetti-netstrada-3400-plus", "olivetti-netstrada-5000",
                "olivetti-netstrada-5200", "olivetti-netstrada-5400",
                "olivetti-netstrada-5400-plus", "olivetti-netstrada-5600",
                "olivetti-netstrada-7000", "olivetti-netstrada-7200", "olivetti-netstrada-7400",
                "olivetti-netstrada-7600", "olivetti-netstrada-9400", "olivetti-notebook-a12",
                "olivetti-notebook-s20", "olivetti-notebook-s25", "olivetti-notebook-s25-c",
                "olivetti-notebook-v16", "olivetti-p500", "olivetti-p700", "olivetti-p750",
                "olivetti-p800", "olivetti-pcs-d4-100-en", "olivetti-pcs-d4-100-n",
                "olivetti-pcs-p-100-en", "olivetti-pcs-p-100-n", "olivetti-pcs-p-133-en",
                "olivetti-pcs-p-133-n", "olivetti-pcs-p-75-e-en", "olivetti-pcs-p-75-educator",
                "olivetti-pcs-p-75-n", "olivetti-pcs-p-90-e-en", "olivetti-pcs-p-90-n",
                "olivetti-pcs30-dx40", "olivetti-pcs30-sx40", "olivetti-pcs40-d250",
                "olivetti-pcs40-d266", "olivetti-pcs40-dx33", "olivetti-pcs40-sx25",
                "olivetti-pcs40-sx33", "olivetti-pcs42", "olivetti-pcs44", "olivetti-pcs44-c",
                "olivetti-pcs5120", "olivetti-pcs5130", "olivetti-pcs5140", "olivetti-pcs52-e",
                "olivetti-pcs6120", "olivetti-pcs6130", "olivetti-pcs6140", "olivetti-pcs6150",
                "olivetti-pcs6160", "olivetti-philos-11", "olivetti-philos-20",
                "olivetti-philos-22", "olivetti-philos-22-color", "olivetti-philos-33",
                "olivetti-philos-33-color", "olivetti-philos-44", "olivetti-philos-44-color",
                "olivetti-philos-45", "olivetti-philos-45-color", "olivetti-philos-46",
                "olivetti-philos-46-color-plus", "olivetti-philos-48",
                "olivetti-philos-48-color-plus", "olivetti-quaderno", "olivetti-quaderno-33",
                "olivetti-snx-140", "olivetti-snx-140-r", "olivetti-snx-140-s", "olivetti-snx-160",
                "olivetti-snx-160-n", "olivetti-snx-160-r", "olivetti-snx-160-rm",
                "olivetti-snx-160-rs", "olivetti-snx-160-s", "olivetti-snx-160e",
                "olivetti-snx-200-rm", "olivetti-snx-200-rs", "olivetti-snx-400-rm",
                "olivetti-snx-400-rs", "olivetti-snx-460-rm", "olivetti-snx-460-rs",
            }, visible)
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
