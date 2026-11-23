# Validation

Updated 2026-11-23 (America/Chicago), v0.2.0.

The length contract is native throughout: source coordinates, periodic boxes,
default radius (0.5), and type radii share the trajectory unit. Header unit labels
do not trigger conversion. Missing labels remain unspecified.

23 reader tests and 5 installer tests pass locally. The reader suite compares
all supported layouts, numeric frame order, float32/float64, compression and
checksums, exact integer labels/genomic intervals, topology and periodic cells
to h5py/NumPy. Unit labels nm, angstrom, reduced, custom and absent must all
preserve coordinate and box lengths. Radius validation covers nonpositive,
nonfinite, overflowing and underflowing values. The removed conversion option
must return an explicit migration error. Failure loops check HDF5 cleanup.

The real VMD length regression uses two beads separated by 1 and a box of side
8. It checks default radius 0.5 and radius/bond ratio 0.5, a global radius override
of 0.8, and a type override of 0.25, for six metadata cases. Invalid radii must
fail before creating a molecule. This test failed on v0.1 with the exact message
"source bond length 1 was rescaled to 10.0" and passes with v0.2.

Run the actual VMD regression (licensed local VMD is required):

```powershell
python tests/test_vmd_lengths.py --vmd 'C:/Program Files/University of Illinois/VMD2/vmd.exe' --package dist/vmd-cndb-0.2.0-windows-x64 --log build/vmd-lengths.log
```

The graphical smoke test also loads through the GUI, sets a global radius, and
switches types to verify radius readback. It covers registration, frames,
coordinates, selections, colors, bonds without distance inference, boxes,
repeated loads, and cleanup. Text and graphical integration pass with the local
VMD Windows alpha (banner 2.0.0a7; internal 2.0.0a6), Tcl 8.6, molfile ABI 18.

A real chromdyn example (32 beads, 100 frames) was compared in native units
against h5py. Maximum absolute difference: 4.76647e-07 source length units,
from the final VMD float32 cast. Local simulation files are not redistributed;
the included 32-bead demo is synthetic. Input-byte identity tests verify that
loading and label overrides do not modify source files.

Windows CI builds the package and runs the reader/installer suites. VMD is not
redistributed and its integration tests run locally. GPU rendering and remote
CNDB streaming are outside these checks.
