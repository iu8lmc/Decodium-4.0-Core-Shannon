#!/usr/bin/env python3
"""Regression checks for Windows DLL search paths and malformed binaries."""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('bundle_dlls', Path(__file__).with_name('check-bundle-dlls.py'))
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class BundlePaths(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / 'plugins').mkdir()
        (self.root / 'other').mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def scan(self, importer, dll, imports):
        importer.write_bytes(b'test')
        dll.write_bytes(b'test')
        def fake_pe(path):
            return imports if Path(path) == importer else []
        output = io.StringIO()
        with patch.object(checker, 'pe_imports', side_effect=fake_pe), \
             patch.object(checker, 'system_has', return_value=False), \
             patch.object(sys, 'argv', ['checker', str(self.root)]), \
             contextlib.redirect_stdout(output):
            return checker.main(), output.getvalue()

    def test_ab_dependency_in_application_root(self):
        for name in ('libcrypto-3-x64.dll', 'libopenjp2-7.dll'):
            with self.subTest(name=name):
                code, _ = self.scan(self.root / 'decodium.exe', self.root / name, [name.upper()])
                self.assertEqual(code, 0)

    def test_ab_unrelated_subdirectory_does_not_satisfy_import(self):
        for name in ('libcrypto-3-x64.dll', 'libopenjp2-7.dll'):
            with self.subTest(name=name):
                code, out = self.scan(self.root / 'decodium.exe', self.root / 'other' / name, [name])
                self.assertEqual(code, 1)
                self.assertIn(name, out)

    def test_plugin_dependency_in_same_directory(self):
        code, _ = self.scan(self.root / 'plugins' / 'plugin.dll', self.root / 'plugins' / 'support.dll', ['support.dll'])
        self.assertEqual(code, 0)

    def test_plugin_dependency_in_different_directory(self):
        code, _ = self.scan(self.root / 'plugins' / 'plugin.dll', self.root / 'other' / 'support.dll', ['support.dll'])
        self.assertEqual(code, 1)

    def test_invalid_pe_fails_closed(self):
        code, out = self.scan(self.root / 'decodium.exe', self.root / 'support.dll', None)
        self.assertEqual(code, 1)
        self.assertIn('INVALID OR UNREADABLE', out)


if __name__ == '__main__':
    unittest.main()
