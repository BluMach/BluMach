# SPDX-License-Identifier: GPL-2.0-or-later
import sys
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import catalog_builder

class ToshibaAlphaProfiles(unittest.TestCase):
    def setUp(self):
        self.catalog, self.locales = catalog_builder.assemble(ROOT/'src/qt/catalog/source')
        self.products = {p['id']:p for p in self.catalog['products']}
    def test_only_two_toshiba_candidates_are_enabled(self):
        visible = {p['id'] for p in self.catalog['products'] if p['manufacturer_id']=='toshiba' and p.get('visible')}
        self.assertEqual({'toshiba-t3200','toshiba-t5200'}, visible)
        self.assertFalse(self.products['toshiba-t5100'].get('visible',False))
        for model in visible:
            self.assertEqual('experimental',self.products[model]['status'])
            for texts in self.locales.values():
                self.assertTrue(texts[self.products[model]['warning_key']])
    def test_default_profiles_are_the_documented_tested_baselines(self):
        for model,cpu,hz,ram,bios in [('toshiba-t3200','286',12000000,1024,'v461'),('toshiba-t5200','i386dx',20000000,2048,'award_v130')]:
            product=self.products[model]
            values={s['section']:s['values'] for s in product['creation']['configuration']}
            self.assertEqual(cpu,values['Machine']['cpu_family'])
            self.assertEqual(hz,values['Machine']['cpu_speed'])
            self.assertEqual(ram,values['Machine']['mem_size'])
            self.assertEqual(0,values['Machine']['cpu_use_dynarec'])
            self.assertEqual('internal',values['Video']['gfxcard'])
            self.assertEqual(bios,values['Toshiba '+model.rsplit('-',1)[1].upper()]['bios'])
            self.assertEqual(0,values['Sound']['mpu401_standalone'])
            self.assertEqual('none',values['Sound']['sndcard'])
    def test_firmware_sets_are_identified_without_bytes(self):
        self.assertEqual(['machines/t3200/IC22_033E.BIN','machines/t3200/IC24_034E.BIN','machines/t3200/AGS_IC8_035C.BIN'],self.products['toshiba-t3200']['firmware']['required_files'])
        self.assertEqual(['machines/t5200/t5200-award-v130.bin','machines/t5200/t5200-vga-1988.bin'],self.products['toshiba-t5200']['firmware']['required_files'])
    def test_disk_images_are_not_created_implicitly(self):
        for model in ['toshiba-t3200','toshiba-t5200']:
            self.assertEqual([],self.products[model]['creation']['generated_files'])
            self.assertNotIn('hard_disk', {f['id'] for f in self.products[model]['creation']['fields']})

if __name__=='__main__': unittest.main()
