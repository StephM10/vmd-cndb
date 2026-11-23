"""Exercise the shipped installer against isolated VMD/configuration fixtures."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

p = argparse.ArgumentParser()
p.add_argument('--package', type=Path, required=True)
a, rest = p.parse_known_args()
PACKAGE = a.package.resolve()
POWERSHELL = shutil.which('pwsh') or shutil.which('powershell')


class InstallerTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.vmd = self.root/'fake VMD'
        self.vmd.mkdir()
        (self.vmd/'vmd.exe').touch()
        self.config = self.vmd/'vmd.rc'
        self.target = self.root/'中文 plugin [x] $y'

    def tearDown(self):
        self.temp.cleanup()

    def invoke(self, script, *args, success=True, env=None):
        r = subprocess.run([POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script), *map(str, args)],
                           capture_output=True, encoding='utf-8', errors='replace', timeout=50, env=env)
        if success: self.assertEqual(r.returncode, 0, r.stdout+r.stderr)
        else: self.assertNotEqual(r.returncode, 0)

    def install(self, **kwargs):
        self.invoke(PACKAGE/'install.ps1', '-VmdDir', self.vmd, '-Destination', self.target,
                    '-StartupFile', self.config, **kwargs)

    def test_idempotent_install_preserves_commands_and_later_edits(self):
        original = b'# existing settings\r\nset my_setting 7\r\n'
        self.config.write_bytes(original)
        self.install()
        manifest = json.loads((self.target/'install-state.json').read_text(encoding='utf-8-sig'))
        self.assertEqual(Path(manifest['startup'][0]['backup']).read_bytes(), original)
        self.install()
        text = self.config.read_text(encoding='utf-8-sig')
        self.assertEqual(text.count('# >>> vmd-cndb managed >>>'), 1)
        self.assertIn('set my_setting 7', text)
        self.assertIn('binary decode hex', text)
        self.assertTrue((self.target/'bin/vmdcndb.dll').is_file())
        self.config.write_text(text+'\n# later user edit\n', encoding='utf-8')
        self.invoke(self.target/'uninstall.ps1')
        after = self.config.read_text(encoding='utf-8-sig')
        self.assertIn('# later user edit', after)
        self.assertIn('set my_setting 7', after)
        self.assertNotIn('vmd-cndb managed', after)

    def test_utf16_startup_keeps_encoding_and_content(self):
        self.config.write_text('# 中文设置\nset custom 1\n', encoding='utf-16')
        self.install()
        self.assertIn('# 中文设置', self.config.read_text(encoding='utf-16'))
        self.invoke(self.target/'uninstall.ps1')
        self.assertIn('set custom 1', self.config.read_text(encoding='utf-16'))

    def test_nonempty_unmanaged_destination_is_rejected(self):
        self.config.write_text('# original\n')
        self.target.mkdir()
        file = self.target/'important.txt'; file.write_text('preserve')
        self.install(success=False)
        self.assertEqual(file.read_text(), 'preserve')
        self.assertEqual(self.config.read_text(), '# original\n')

    def test_malformed_block_is_rejected(self):
        self.config.write_text('# >>> vmd-cndb managed >>>\n# missing end marker\n')
        before = self.config.read_bytes()
        self.install(success=False)
        self.assertEqual(self.config.read_bytes(), before)

    def test_default_user_install_sources_defaults_and_creates_launcher(self):
        self.config.write_text('# VMD defaults\nset default_value 1\n')
        personal = self.root/'personal home'; personal.mkdir()
        roaming = self.root/'roaming'
        env = dict(os.environ, HOME=str(personal), USERPROFILE=str(personal), APPDATA=str(roaming))
        self.invoke(PACKAGE/'install.ps1', '-VmdDir', self.vmd, '-Destination', self.target, env=env)
        text = (personal/'vmd.rc').read_text(encoding='utf-8-sig')
        self.assertIn('source [encoding convertfrom utf-8', text)
        self.assertIn('# >>> vmd-cndb managed >>>', text)
        self.assertEqual(self.config.read_text(), '# VMD defaults\nset default_value 1\n')
        self.assertTrue((roaming/'Microsoft/Windows/Start Menu/Programs/VMD CNDB Loader.lnk').is_file())


if __name__ == '__main__': unittest.main(argv=[__file__, *rest])
