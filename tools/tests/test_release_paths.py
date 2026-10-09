# SPDX-License-Identifier: GPL-2.0-or-later
"""Guard product-owned defaults and absence of automatic upstream-root reuse."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ReleasePathTests(unittest.TestCase):
    def test_product_owned_library_key_for_both_config_loaders_and_save(self):
        header = (ROOT / "src/include/86box/86box.h").read_text(encoding="utf-8")
        source = (ROOT / "src/config.c").read_text(encoding="utf-8")
        self.assertRegex(header, r'#define\s+VMM_CONFIG_KEY\s+"blumach_vmm_path"')
        self.assertEqual(source.count('ini_section_get_string(cat, VMM_CONFIG_KEY, NULL)'), 2)
        self.assertEqual(source.count('ini_section_set_string(cat, VMM_CONFIG_KEY,'), 2)
        self.assertIn('ini_section_delete_var(cat, VMM_CONFIG_KEY)', source)
        self.assertNotRegex(source, r'ini_section_(?:get|set|delete)\w*\([^\n]*"vmm_path"')

    def test_windows_and_portable_defaults_are_owned_by_blumach(self):
        header = (ROOT / "src/include/86box/86box.h").read_text(encoding="utf-8")
        for macro in ('VMM_PATH', 'VMM_PATH_WINDOWS'):
            self.assertRegex(header, rf'#define\s+{macro}\s+"BluMach VMs"')
        platform = (ROOT / "src/qt/qt_platform.cpp").read_text(encoding="utf-8")
        self.assertIn('QDir::home().filePath(VMM_PATH_WINDOWS)', platform)
        self.assertIn('QDir(exe_path).filePath(VMM_PATH)', platform)
        self.assertIn('QStandardPaths::AppConfigLocation', platform)
        self.assertIn('QStandardPaths::AppDataLocation', platform)

    def test_product_filenames_and_explicit_read_only_legacy_input(self):
        header = (ROOT / "src/include/86box/86box.h").read_text(encoding="utf-8")
        self.assertRegex(header, r'#define\s+CONFIG_FILE\s+"blumach.cfg"')
        self.assertRegex(header, r'#define\s+GLOBAL_CONFIG_FILE\s+"blumach_global.cfg"')
        scanner = (ROOT / "src/qt/qt_vmmanager_system.cpp").read_text(encoding="utf-8")
        writer = (ROOT / "src/qt/qt_vmmanager_main.cpp").read_text(encoding="utf-8")
        importer = (ROOT / "src/qt/qt_vmmanager_addmachine.cpp").read_text(encoding="utf-8")
        self.assertIn('QString(CONFIG_FILE)', scanner)
        self.assertIn('newSystemDirectory.path() + "/" + CONFIG_FILE', writer)
        self.assertIn('QIODevice::ReadOnly | QIODevice::Text', importer)
        self.assertIn('configuration.file_filter', importer)
        guide = (ROOT / "doc/releases/0.1.0-alpha.1.md").read_text(encoding="utf-8")
        self.assertIn('not moved, deleted, renamed or copied', guide)
        self.assertIn('absolute media paths', guide)


if __name__ == '__main__':
    unittest.main()
