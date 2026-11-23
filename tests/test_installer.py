"""Test the standard VMD directory layout in isolated writable fixtures."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--package', type=Path, required=True)
a, rest = parser.parse_known_args()
PACKAGE = a.package.resolve()
POWERSHELL = shutil.which('pwsh') or shutil.which('powershell')

class InstallerTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.vmd = self.root/'中文 VMD [x] $y'
        self.vmd.mkdir()
        (self.vmd/'vmd.exe').write_bytes(b'original executable fixture')
        (self.vmd/'vmd.rc').write_text('# original startup\n')
        self.personal = self.root/'vmd.rc'
        self.target = self.vmd/'plugins/noarch/tcl/vmdcndb0.2.1'
        self.native = self.vmd/'plugins/WIN64/molfile/vmdcndb.so'
        self.hook = self.vmd/'scripts/init.d/vmdcndb.tcl'

    def tearDown(self): self.temp.cleanup()

    def invoke(self, script, *args, success=True):
        result = subprocess.run([POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script), *map(str,args)],
                                capture_output=True, encoding='utf-8', errors='replace', timeout=50)
        if success: self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        else: self.assertNotEqual(result.returncode,0)

    def install(self, **kwargs):
        self.invoke(PACKAGE/'install.ps1','-VmdDir',self.vmd,'-LegacyStartupFile',self.personal,**kwargs)

    def test_standard_layout_idempotency_and_stock_files_unchanged(self):
        self.install();self.install()
        self.assertEqual((self.vmd/'vmd.exe').read_bytes(),b'original executable fixture')
        self.assertEqual((self.vmd/'vmd.rc').read_text(),'# original startup\n')
        self.assertFalse(self.personal.exists())
        self.assertTrue((self.target/'pkgIndex.tcl').is_file())
        self.assertEqual(self.native.read_bytes(),(PACKAGE/'bin/vmdcndb.dll').read_bytes())
        self.assertIn('vmd_install_extension',self.hook.read_text())
        state=json.loads((self.target/'install-state.json').read_text(encoding='utf-8-sig'))
        self.assertEqual(Path(state['native']),self.native)

    def test_legacy_migration_preserves_defaults_and_user_commands(self):
        original='# retain defaults\nsource {C:/VMD/vmd.rc}\n# >>> vmd-cndb managed >>>\nset cndb_bootstrap old\n# <<< vmd-cndb managed <<<\n# later user command\n'
        self.personal.write_text(original,encoding='utf-8')
        self.install()
        text=self.personal.read_text(encoding='utf-8-sig')
        self.assertIn('source {C:/VMD/vmd.rc}',text)
        self.assertIn('# later user command',text)
        self.assertNotIn('vmd-cndb managed',text)
        state=json.loads((self.target/'install-state.json').read_text(encoding='utf-8-sig'))
        self.assertEqual(Path(state['legacy_backup']).read_text(encoding='utf-8'),original)

    def test_utf16_legacy_migration_keeps_encoding(self):
        self.personal.write_text('# 中文设置\n# >>> vmd-cndb managed >>>\nold\n# <<< vmd-cndb managed <<<\n',encoding='utf-16')
        self.install()
        self.assertEqual(self.personal.read_text(encoding='utf-16'),'# 中文设置\n')

    def test_unmanaged_directory_is_preserved(self):
        self.target.mkdir(parents=True)
        important=self.target/'important.txt';important.write_text('preserve')
        self.install(success=False)
        self.assertEqual(important.read_text(),'preserve')
        self.assertFalse(self.native.exists())

    def test_malformed_legacy_config_fails_before_installing(self):
        self.personal.write_text('# >>> vmd-cndb managed >>>\n# missing end\n')
        self.install(success=False)
        self.assertFalse(self.native.exists());self.assertFalse(self.hook.exists())

    def test_uninstall_disables_discovery_and_preserves_stock_and_user_files(self):
        self.install()
        (self.target/'user-note.txt').write_text('preserve')
        self.invoke(self.target/'uninstall.ps1')
        self.assertFalse(self.hook.exists());self.assertFalse(self.native.exists())
        self.assertFalse((self.target/'pkgIndex.tcl').exists())
        self.assertEqual((self.target/'user-note.txt').read_text(),'preserve')
        self.assertEqual((self.vmd/'vmd.rc').read_text(),'# original startup\n')

    def test_uninstall_preserves_modified_registration(self):
        self.install();self.hook.write_text('# user modification\n')
        self.invoke(self.target/'uninstall.ps1',success=False)
        self.assertTrue(self.native.exists())
        self.assertEqual(self.hook.read_text(),'# user modification\n')

if __name__ == '__main__': unittest.main(argv=[__file__,*rest])
