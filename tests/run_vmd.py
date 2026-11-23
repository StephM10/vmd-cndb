"""Run real VMD integration checks without requiring VMD in CI."""
import argparse
from pathlib import Path
import os
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--vmd', type=Path, required=True)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--extra', type=Path)
p.add_argument('--gui', action='store_true')
p.add_argument('--normal-startup', action='store_true')
p.add_argument('--startup-file', type=Path, help='Use the same explicit startup file as the installed Start menu shortcut')
p.add_argument('--log', type=Path, required=True)
a = p.parse_args()
env = dict(os.environ, VMDDIR=str(a.vmd.resolve().parent), CNDB_PACKAGE=str(a.package.resolve()))
if a.extra: env['CNDB_EXTRA_SMOKE'] = str(a.extra.resolve())
if a.normal_startup: env['CNDB_REQUIRE_AUTOSTART'] = '1'
cmd = [str(a.vmd), '-e', str(Path(__file__).with_name('vmd_smoke.tcl').resolve())]
if not a.gui: cmd += ['-dispdev', 'text']
if a.startup_file: cmd += ['-startup', str(a.startup_file.resolve())]
elif not a.normal_startup: cmd += ['-startup', 'NUL']
result = subprocess.run(cmd, env=env, capture_output=True, timeout=90)
text = (result.stdout + result.stderr).decode('utf-8', errors='replace')
a.log.write_text(text, encoding='utf-8')
if 'CNDB_SMOKE_OK' not in text or 'CNDB_SMOKE_FAILED' in text:
    print(text[-12000:].encode('ascii', errors='backslashreplace').decode('ascii')); raise SystemExit(1)
if a.gui and 'CNDB_GUI_OK' not in text: raise SystemExit('GUI did not initialize')
print('VMD integration checks passed; log:', a.log)
