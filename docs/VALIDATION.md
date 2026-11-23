# Validation

Initial validation date: 2026-11-22 (America/Chicago).

The black-box reader suite compares the compiled C++ executable to h5py/NumPy
for layouts, frame order, f32/f64 coordinates, unit scaling, gzip/shuffle/checksum
decoding, exact large integer labels/genomic intervals, external labels,
topology, and box geometry. Failure loops verify HDF5 object counts return to
zero after invalid files and invalid metadata. A corrupted late frame must fail
without returning partial JSON.

The installer suite checks idempotency, backup creation, managed block removal,
preservation of existing commands and later edits, and paths containing spaces
and Unicode. CI runs these suites and builds the downloadable Windows package.

The real VMD smoke test checks plugin registration, selected frames, nm→Å,
full-label selection, independent type colors/radii, linear bonds without
distance inference, periodic boxes, frame changes, repeated loads, and metadata
cleanup on deletion. It can additionally construct the Tk window and require
automatic initialization through normal startup.

Local results: 20 reader tests and 5 installer tests pass. Text and graphical
VMD integration pass, including the registered extension menu and the installed
Start menu launcher's startup file. The real chromdyn example's 100 frames agree
with h5py after conversion to Å; maximum absolute coordinate difference is
3.82×10⁻⁶ Å, within the final float32 cast.

Local integration also uses a real chromdyn example (32 beads, 100 frames) and
a multi-trajectory CNDB v2 fixture, with a late invalid frame followed by a valid
load to verify rollback and transaction cleanup. Local simulation files are not
included in the public repository. The included 32-bead demo is synthetic.

Compatibility is established for the local VMD Windows alpha (banner 2.0.0a7,
internal 2.0.0a6), not all VMD releases. GPU rendering and remote CNDB IO were not
tested or implemented. File-byte identity checks confirm loading and label
overrides leave input CNDB files unchanged.
