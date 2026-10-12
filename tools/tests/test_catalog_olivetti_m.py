# SPDX-License-Identifier: GPL-2.0-or-later
"""Commercial grouping must not collapse model identities or replace filters."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import catalog_builder


class OlivettiMRangeTests(unittest.TestCase):
    def setUp(self):
        self.catalog, self.locales = catalog_builder.assemble(ROOT / 'src/qt/catalog/source')
        self.models = [p for p in self.catalog['products']
                       if p['manufacturer_id'] == 'olivetti' and p['name'].startswith('Olivetti M')]

    def test_m_models_are_direct_members_of_one_manufacturer_owned_range(self):
        family = next(f for f in self.catalog['families'] if f['id'] == 'olivetti-m')
        self.assertEqual('olivetti', family['manufacturer_id'])
        self.assertEqual('', family['parent_family_id'])
        self.assertEqual('M', family['name'])
        self.assertGreaterEqual(len(self.models), 13)
        for product in self.models:
            with self.subTest(model=product['id']):
                self.assertEqual('olivetti-m', product['family_id'])

    def test_number_based_groups_are_not_extra_tree_levels(self):
        ids = {f['id'] for f in self.catalog['families']}
        self.assertFalse(ids & {'olivetti-m15', 'olivetti-m28', 'olivetti-m280',
                                'olivetti-m250', 'olivetti-m300', 'olivetti-m380'})
        self.assertTrue({'olivetti-m', 'olivetti-pcs', 'olivetti-prodest'} <= ids)

    def test_distinct_models_and_facets_remain_independent(self):
        products = {p['id']: p for p in self.catalog['products']}
        self.assertNotEqual(products['olivetti-m250']['platform_id'], products['olivetti-m250e']['platform_id'])
        self.assertIn('laptop', products['olivetti-m15']['facets']['form_factor'])
        self.assertNotEqual(products['olivetti-m15']['facets']['form_factor'],
                            products['olivetti-m250']['facets']['form_factor'])
        if 'olivetti-m15-plus' in products:
            self.assertEqual('olivetti-m', products['olivetti-m15-plus']['family_id'])
            self.assertNotEqual(products['olivetti-m15']['platform_id'],
                                products['olivetti-m15-plus']['platform_id'])

    def test_description_exists_in_every_catalogue_language(self):
        family = next(f for f in self.catalog['families'] if f['id'] == 'olivetti-m')
        for locale, texts in self.locales.items():
            with self.subTest(locale=locale):
                self.assertTrue(texts[family['description_key']].strip())


if __name__ == '__main__':
    unittest.main()
