# SPDX-License-Identifier: GPL-2.0-or-later
import sys
import re
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import catalog_builder

class ToshibaAlphaProfiles(unittest.TestCase):
    def setUp(self):
        self.catalog, self.locales = catalog_builder.assemble(ROOT/'src/qt/catalog/source')
        self.products = {p['id']:p for p in self.catalog['products']}
    def test_all_three_toshiba_candidates_are_enabled(self):
        visible = {p['id'] for p in self.catalog['products'] if p['manufacturer_id']=='toshiba' and p.get('visible')}
        self.assertEqual({'toshiba-t3200','toshiba-t5100','toshiba-t5200'}, visible)
        for model in visible:
            self.assertEqual('experimental',self.products[model]['status'])
            for texts in self.locales.values():
                self.assertTrue(texts[self.products[model]['warning_key']])
    def test_default_profiles_are_the_documented_tested_baselines(self):
        for model,cpu,hz,ram,bios in [('toshiba-t3200','286',12000000,1024,'v461'),('toshiba-t5100','i386dx',16000000,2048,'v230'),('toshiba-t5200','i386dx',20000000,2048,'award_v130')]:
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
        required=['machines/t5100/Toshiba T5100 - BIOS ROM - V2.30 - EVEN - 042F - 27C256.bin','machines/t5100/Toshiba T5100 - BIOS ROM - V2.30 - ODD - 043F - 27C256.bin','video/ega/ibm_6277356_ega_card_u44_27128.bin']
        self.assertEqual(required,self.products['toshiba-t5100']['firmware']['required_files'])
        native=(ROOT/'src/machine/m_at_t5100.c').read_text(encoding='utf-8')
        self.assertRegex(native,r'\.files_no\s*=\s*3,')
        for name in required:
            self.assertIn('roms/'+name,native)
        source=(ROOT/'src/machine/m_at_toshiba_t5200.c').read_text(encoding='utf-8')
        self.assertRegex(source, r'\.files_no\s*=\s*2,\s*\.local\s*=\s*0,\s*\.size\s*=\s*131072,\s*\.files\s*=\s*\{\s*"roms/machines/t5200/t5200-award-v130.bin",\s*"roms/machines/t5200/t5200-vga-1988.bin"')
    def test_disk_images_are_not_created_implicitly(self):
        for model in ['toshiba-t3200','toshiba-t5200']:
            self.assertEqual([],self.products[model]['creation']['generated_files'])
            self.assertNotIn('hard_disk', {f['id'] for f in self.products[model]['creation']['fields']})

    def test_t5100_compatibility_profile_is_explicit_and_bounded(self):
        product=self.products['toshiba-t5100']
        profile=product['creation']
        self.assertEqual([{'path':'toshiba-t5100-cp3044.img','size':980*5*17*512}],profile['generated_files'])
        values={s['section']:s['values'] for s in profile['configuration']}
        self.assertEqual('17, 5, 980, 0, ide',values['Hard disks']['hdd_01_parameters'])
        memory=next(f for f in profile['fields'] if f['id']=='memory')
        self.assertEqual('2mb',memory['default'])
        self.assertEqual('unavailable',next(c for c in memory['choices'] if c['id']=='4mb')['status'])
        for texts in self.locales.values():
            self.assertIn('IBM EGA',texts[product['warning_key']])
            self.assertIn('AGS',texts[product['warning_key']])
            self.assertIn('AGS',texts[profile['note_keys'][0]])

if __name__=='__main__': unittest.main()
