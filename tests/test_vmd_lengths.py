"""Regression: source coordinates, box, and bead radii share one length unit."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

import h5py
import numpy as np

p = argparse.ArgumentParser()
p.add_argument('--vmd', type=Path, required=True)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--log', type=Path, required=True)
a = p.parse_args()
with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    for name, units in [('legacy', None), ('nm', 'nanometer'), ('angstrom', 'angstrom'), ('reduced', 'reduced'), ('custom', 'sigma'), ('missing', '')]:
        with h5py.File(root/f'{name}.cndb', 'w') as f:
            if units is not None:
                h = f.create_group('Header')
                h.attrs.update(format_name='OpenMiChroM-CNDB', format_version='2.0')
                if units: h.attrs['coordinate_units'] = units
            f.create_dataset('types', data=np.asarray(['A', 'B'], dtype=h5py.string_dtype()))
            d = f.create_dataset('0', data=np.asarray([[0., 0, 0], [1., 0, 0]]))
            d.attrs['box'] = np.eye(3)*8
    env = dict(os.environ, VMDDIR=str(a.vmd.resolve().parent), CNDB_PACKAGE=str(a.package.resolve()), CNDB_LENGTH_FIXTURES=str(root))
    result = subprocess.run([str(a.vmd), '-dispdev', 'text', '-startup', 'NUL', '-e', str(Path(__file__).with_name('vmd_lengths.tcl').resolve())],
                            env=env, capture_output=True, timeout=45)
    text = (result.stdout+result.stderr).decode('utf-8', errors='replace')
    a.log.write_text(text, encoding='utf-8')
    if 'CNDB_LENGTHS_OK' not in text or 'CNDB_LENGTHS_FAILED' in text:
        print(text[-6000:].encode('ascii', 'backslashreplace').decode()); raise SystemExit(1)
    print('Native length units, default/overridden/type radii, and invalid radii: passed.')
