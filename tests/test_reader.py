"""Black-box comparisons of the compiled reader against h5py/NumPy.

Run: python tests/test_reader.py --exe build/plugin/Release/cndb_check.exe
VMD is deliberately not required for these distributable tests.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

import h5py
import numpy as np

PARSER = argparse.ArgumentParser()
PARSER.add_argument('--exe', type=Path, required=True)
ARGS, REMAINING = PARSER.parse_known_args()
EXE = ARGS.exe.resolve()
UTF8 = h5py.string_dtype('utf-8')


def write(path, *, types=('A1', 'B1', 'custom'), version=False, grouped=False,
          dtype='f8', compressed=False, frames=('10', '0', '2'), units='nanometer',
          topology=False, box=False):
    reference = {}
    with h5py.File(path, 'w') as f:
        if version:
            h = f.create_group('Header')
            h.attrs.update(format_name='OpenMiChroM-CNDB', format_version='2.0',
                           coordinate_units=units, indexed=True,
                           n_frames=np.int64(len(frames)))
            f.create_dataset('_index', data=np.void(b'ignored local index'))
        groups = ('arbitrary replica name', 'replica2_chr7') if grouped else (None,)
        for gi, name in enumerate(groups):
            g = f.create_group(name) if name else f
            g.create_dataset('types', data=np.asarray(types, dtype=UTF8 if isinstance(types[0], str) else None))
            if version:
                g.create_dataset('genomic_position', data=np.array([[2**53 + 3, 2**53 + 10], [40, 50], [60, 70]], dtype='i8'))
            coords = g.create_group('spatial_position') if name else g
            for frame in frames:
                x = (np.arange(9).reshape(3, 3) / 7 + int(frame.removeprefix('t_')) + gi * 100).astype(dtype)
                ds = coords.create_dataset(frame, data=x,
                                           compression='gzip' if compressed else None,
                                           shuffle=compressed, fletcher32=compressed)
                ds.attrs['time'] = float(int(frame.removeprefix('t_'))) / 2
                ds.attrs['time_units'] = 'picosecond'
                if box:
                    ds.attrs['box'] = np.array([[8., 0, 0], [1, 9, 0], [2, 1, 10]])
                reference[(name or '/', frame)] = x
            if topology:
                t = g.create_group('topology')
                dt = np.dtype([('name', 'S20'), ('element', 'S5'), ('res_idx', 'i4'), ('chain_idx', 'i4')])
                t.create_dataset('atoms', data=np.array([(b'CA', b'X', 0, 0), (b'B', b'X', 1, 1), (b'C', b'X', 1, 1)], dtype=dt))
                t.create_dataset('chain_ids', data=np.array(['X', 'chain long'], dtype=UTF8))
                t.create_dataset('res_names', data=np.array(['R1', 'R2'], dtype=UTF8))
                t.create_dataset('bonds', data=np.array([[1, 2]], dtype='i4'))
    return reference


class ReaderTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.file = self.root / 'test.cndb'

    def tearDown(self):
        self.tmp.cleanup()

    def invoke(self, command='read', path=None, *options, success=True):
        result = subprocess.run([str(EXE), command, str(path or self.file), *map(str, options)],
                                capture_output=True, encoding='utf-8', timeout=40)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
            return json.loads(result.stdout)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('CNDB:', result.stderr)
        self.assertEqual(result.stdout, '')
        return result.stderr

    def test_legacy_numeric_order_and_units(self):
        ref = write(self.file)
        result = self.invoke()
        self.assertEqual([f['id'] for f in result['frames']], ['0', '2', '10'])
        self.assertEqual(result['types'], ['A1', 'B1', 'custom'])
        self.assertEqual(result['bonds'], [[1, 2], [2, 3]])
        for frame in result['frames']:
            np.testing.assert_array_equal(np.array(frame['xyz'], dtype='f4').reshape(3, 3), (ref['/', frame['id']] * 10).astype('f4'))

    def test_float32_gzip_shuffle_fletcher32(self):
        ref = write(self.file, dtype='f4', compressed=True)
        for frame in self.invoke()['frames']:
            np.testing.assert_array_equal(np.array(frame['xyz'], dtype='f4').reshape(3, 3), (ref['/', frame['id']].astype('f8') * 10).astype('f4'))

    def test_v2_root_header_genomic_and_time(self):
        write(self.file, version=True, units='angstrom')
        r = self.invoke()
        self.assertEqual(r['scale'], 1)
        self.assertEqual(r['genomic_position'][0], [2**53 + 3, 2**53 + 10])
        self.assertEqual(r['header']['indexed']['values'], ['1'])
        self.assertEqual(r['frames'][-1]['time_ps'], 5)
        self.assertEqual(r['frames'][-1]['attributes']['time']['values'], ['5'])

    def test_multi_trajectory_requires_selection(self):
        ref = write(self.file, version=True, grouped=True, frames=('t_10', 't_0', 't_2'))
        choices = self.invoke('inspect')
        self.assertEqual([s['name'] for s in choices], ['/arbitrary replica name', '/replica2_chr7'])
        self.assertIn('multiple trajectories', self.invoke(success=False))
        r = self.invoke('read', None, '--trajectory', 'replica2_chr7')
        self.assertEqual([f['id'] for f in r['frames']], ['t_0', 't_2', 't_10'])
        for frame in r['frames']:
            np.testing.assert_array_equal(np.array(frame['xyz'], dtype='f4').reshape(3, 3), (ref['replica2_chr7', frame['id']] * 10).astype('f4'))

    def test_frame_range_selects_sorted_positions(self):
        write(self.file)
        r = self.invoke('read', None, '--first', 0, '--last', 2, '--stride', 2)
        self.assertEqual([f['id'] for f in r['frames']], ['0', '10'])
        for options in [('--stride', 0), ('--first', -1), ('--last', 3), ('--first', 2, '--last', 0)]:
            self.invoke('read', None, *options, success=False)

    def test_numeric_types_preserve_identity(self):
        write(self.file, types=np.array([0, 6, 999], dtype='i8'))
        r = self.invoke()
        self.assertEqual(r['types'], ['0', '6', '999'])
        self.assertEqual(r['type_kind'], 'integer')
        self.assertEqual(r['aliases'], r['types'])

    def test_large_unsigned_types(self):
        write(self.file, types=np.array([2**64-1, 2**53+3, 7], dtype='u8'))
        r = self.invoke()
        self.assertEqual(r['types'], [str(2**64-1), str(2**53+3), '7'])

    def test_fixed_string_types(self):
        write(self.file)
        with h5py.File(self.file, 'r+') as f:
            del f['types']; f.create_dataset('types', data=np.array([b'X', b'Y', b'Z'], dtype='S8'))
        self.assertEqual(self.invoke()['types'], ['X', 'Y', 'Z'])

    def test_external_labels_and_collision_free_aliases(self):
        write(self.file)
        labels = self.root / 'labels.tsv'
        labels.write_text('\ufeffbead_id\ttype\n3\tcustom long name [x] $y\n1\tT0\n2\t染色质类型\n', encoding='utf-8')
        before = self.file.read_bytes()
        r = self.invoke('read', None, '--labels', labels)
        self.assertEqual(r['types'], ['T0', '染色质类型', 'custom long name [x] $y'])
        self.assertEqual(r['raw_types'], ['A1', 'B1', 'custom'])
        self.assertEqual(r['aliases'][0], 'T0')
        self.assertEqual(len(set(r['aliases'])), 3)
        self.assertTrue(all(len(s.encode()) <= 15 for s in r['aliases']))
        self.assertEqual(self.file.read_bytes(), before)

    def test_labels_require_complete_unique_ids(self):
        write(self.file)
        labels = self.root / 'bad.tsv'
        for body in ['1\tX\n1\tY\n3\tZ\n', '1\tX\n2\tY\n', '0\tX\n2\tY\n3\tZ\n', '1\t\n2\tY\n3\tZ\n', '1\tX\tx\n2\tY\n3\tZ\n']:
            labels.write_text('bead_id\ttype\n'+body)
            self.invoke('read', None, '--labels', labels, success=False)
            self.invoke('failure-loop', None, '--labels', labels)

    def test_embedded_topology_and_periodic_cell(self):
        write(self.file, topology=True, box=True)
        r = self.invoke()
        self.assertEqual(r['bonds'], [[2, 3]])
        self.assertEqual(r['atoms'][1], dict(name='B', resname='R2', chain='chain long', resid=2))
        box = np.array([[8., 0, 0], [1, 9, 0], [2, 1, 10]])
        lengths = np.linalg.norm(box, axis=1)
        angles = [np.degrees(np.arccos(np.dot(box[i], box[j]) / lengths[i] / lengths[j])) for i, j in [(1, 2), (0, 2), (0, 1)]]
        np.testing.assert_allclose(r['frames'][0]['cell'], [*list(lengths*10), *angles], rtol=1e-6)
        np.testing.assert_array_equal(np.array(r['frames'][0]['box_angstrom']).reshape(3, 3), box*10)

    def test_explicit_psf_overrides_embedded_topology(self):
        write(self.file, topology=True)
        path = self.root / 'ring.psf'
        path.write_text('PSF EXT\n\n3 !NATOM\n1 S1 1 BEAD A X 0 1\n2 S1 2 BEAD B X 0 1\n3 S1 3 BEAD C X 0 1\n3 !NBOND: bonds\n1 2 2 3 3 1\n')
        r = self.invoke('read', None, '--psf', path)
        self.assertEqual(r['bonds'], [[1, 2], [2, 3], [3, 1]])
        self.assertEqual(r['types'], ['A1', 'B1', 'custom'])

    def test_rotated_box_preserves_periodic_geometry(self):
        ref = write(self.file)
        box = np.array([[0., 8, 0], [-9, 1, 0], [-1, 2, 10]])
        r = self.invoke('read', None, '--box', ','.join(map(str, box.ravel())))
        f = r['frames'][0]
        xyz = np.array(f['xyz']).reshape(3, 3)
        rotation = np.array(f['rotation']).reshape(3, 3)
        np.testing.assert_allclose(xyz, ref['/', '0'] @ rotation.T * 10, rtol=2e-7)
        np.testing.assert_allclose(np.linalg.norm(xyz[1]-xyz[0]), np.linalg.norm(ref['/', '0'][1]-ref['/', '0'][0])*10, rtol=2e-7)

    def test_empty_embedded_bonds_remain_empty(self):
        write(self.file, topology=True)
        with h5py.File(self.file, 'r+') as f: del f['topology/bonds']
        self.assertEqual(self.invoke()['bonds'], [])

    def test_unicode_spaces_and_tcl_punctuation_in_paths(self):
        folder = self.root / '中文 路径 [x] $y; braces{}'
        folder.mkdir()
        path = folder / '轨迹 文件.cndb'
        write(path)
        self.assertEqual(self.invoke('read', path)['beads'], 3)

    def test_repeated_loads_close_all_hdf5_objects(self):
        write(self.file, topology=True, version=True, compressed=True, box=True)
        self.invoke('read', None, '--stress', 25)

    def test_nan_late_frame_fails_without_partial_output(self):
        write(self.file)
        with h5py.File(self.file, 'r+') as f: f['10'][2, 1] = np.nan
        self.assertIn('nonfinite', self.invoke(success=False))
        self.invoke('failure-loop')

    def test_invalid_shape_type_bonds_and_units(self):
        for mode in ['shape', 'types', 'bonds', 'units', 'box', 'duplicates']:
            write(self.file, topology=True, version=True)
            with h5py.File(self.file, 'r+') as f:
                if mode == 'shape':
                    del f['2']; f.create_dataset('2', data=np.zeros((2, 3)))
                elif mode == 'types':
                    del f['types']; f.create_dataset('types', data=np.array([1, 2]))
                elif mode == 'bonds': f['topology/bonds'][0, 1] = 10
                elif mode == 'units': f['Header'].attrs['coordinate_units'] = 'unknown'
                elif mode == 'box': f['0'].attrs['box'] = np.zeros((3, 3))
                elif mode == 'duplicates': f.create_dataset('t_2', data=np.zeros((3, 3)))
            self.invoke(success=False)
            self.invoke('failure-loop')

    def test_damaged_file_and_missing_path(self):
        self.file.write_bytes(b'not hdf5')
        self.invoke(success=False)
        self.invoke('failure-loop')
        self.invoke('read', self.root / 'missing.cndb', success=False)

    def test_missing_units_requires_explicit_override(self):
        write(self.file, version=True)
        with h5py.File(self.file, 'r+') as f: del f['Header'].attrs['coordinate_units']
        self.invoke(success=False)
        self.assertEqual(self.invoke('read', None, '--units', 'nm')['scale'], 10)


if __name__ == '__main__':
    unittest.main(argv=[__file__, *REMAINING])
