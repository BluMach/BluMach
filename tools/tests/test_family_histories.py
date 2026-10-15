# SPDX-License-Identifier: GPL-2.0-or-later
"""Family histories add context without changing product availability."""
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import catalog_builder

SOURCE = Path(__file__).resolve().parents[2] / 'src/qt/catalog/source'


class FamilyHistoriesTests(unittest.TestCase):
    def setUp(self):
        self.catalog, self.translations = catalog_builder.assemble(SOURCE)
        self.families = {f['id']: f for f in self.catalog['families']}

    def test_tables_are_complete_and_all_cells_resolve_in_five_locales(self):
        for family_id, count, refs in [('olivetti-m', 24, 3), ('toshiba-t-series', 9, 5)]:
            family = self.families[family_id]
            table = family['comparison_table']
            self.assertEqual(len(table['columns']), 5)
            self.assertEqual(len(table['rows']), count)
            self.assertEqual(len(family['history_references']), refs)
            self.assertEqual(len({r['id'] for r in table['rows']}), count)
            keys = [family['description_key'], family['history_key'], table['title_key'], table['note_key']]
            keys += [c['label_key'] for c in table['columns']]
            for row in table['rows']:
                self.assertEqual(len(row['cells']), 5)
                keys += [cell['value_key'] for cell in row['cells'] if 'value_key' in cell]
            self.assertEqual(set(self.translations), {'en', 'es', 'fr', 'it', 'pt'})
            for locale, values in self.translations.items():
                for key in keys:
                    with self.subTest(family=family_id, locale=locale, key=key):
                        self.assertTrue(values[key].strip())
                        self.assertNotIn('\ufffd', values[key])
                        self.assertFalse(values[key].startswith('family.'))
                self.assertGreater(len(values[family['history_key']].split()), 170)

    def test_alpha_rows_match_visible_products_not_historical_references(self):
        for family_id, expected in [('olivetti-m', {'m15', 'm15-plus', 'm28'}), ('toshiba-t-series', {'t3200', 't5100', 't5200'})]:
            rows = self.families[family_id]['comparison_table']['rows']
            enabled = {r['id'] for r in rows if r['cells'][4].get('value_key', '').endswith(('.status.alpha', '.status.partial'))}
            self.assertEqual(enabled, expected)
            visible = {p['id'] for p in self.catalog['products'] if p.get('visible') and p['family_id'] == family_id}
            prefix = 'olivetti-' if family_id == 'olivetti-m' else 'toshiba-'
            self.assertEqual(visible, {prefix + model for model in enabled})
            for row in rows:
                self.assertNotIn('creation', row)
                self.assertNotIn('machine_id', row)

    def test_undated_models_follow_dated_models_and_variants_remain_distinct(self):
        for family_id in ['olivetti-m', 'toshiba-t-series']:
            rows = self.families[family_id]['comparison_table']['rows']
            unknown_seen = False
            for row in rows:
                unknown = row['cells'][0].get('value_key', '').endswith('.period.unknown')
                if unknown_seen:
                    self.assertTrue(unknown)
                unknown_seen |= unknown
            self.assertTrue(unknown_seen)
        trows = {r['id']: r for r in self.families['toshiba-t-series']['comparison_table']['rows']}
        self.assertNotEqual(trows['t3200']['cells'][2], trows['t3200sx']['cells'][2])
        self.assertNotEqual(trows['t5200']['cells'][3], trows['t5200c']['cells'][3])


if __name__ == '__main__':
    unittest.main()
