"""Generate a small, deterministic simulation demo. Developer dependency: h5py."""
from pathlib import Path
import h5py
import numpy as np

root = Path(__file__).resolve().parent
n = 32
t = np.linspace(0, 3*np.pi, n)
with h5py.File(root/'demo.cndb', 'w') as f:
    header = f.create_group('Header')
    header.attrs.update(format_name='OpenMiChroM-CNDB', format_version='2.0',
                        coordinate_units='nanometer', creator='vmd-cndb demo',
                        n_beads=n, n_frames=20)
    f.create_dataset('types', data=np.asarray(['active domain' if i < 16 else 'B1' for i in range(n)], dtype=h5py.string_dtype()))
    for frame in range(20):
        theta = t + 0.06*frame
        xyz = np.column_stack((2*np.cos(theta), 2*np.sin(theta), np.linspace(-3, 3, n)))
        ds = f.create_dataset(str(frame), data=xyz.astype('f4'), compression='gzip')
        ds.attrs['time_ps'] = frame*0.5
        ds.attrs['box'] = np.eye(3)*12
with (root/'labels.tsv').open('w', encoding='utf-8', newline='\n') as out:
    out.write('bead_id\ttype\n')
    for i in range(n): out.write(f'{i+1}\t'+('custom active type' if i < 16 else 'B1')+'\n')
