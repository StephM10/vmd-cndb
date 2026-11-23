# Supported CNDB schema

Baseline: OpenMiChroM
[`b7498f2589410b582e94befd779b0749f87a0c1a`](https://github.com/junioreif/OpenMiChroM/blob/b7498f2589410b582e94befd779b0749f87a0c1a/docs/source/Developer/structural_file_formats.rst).

Legacy files contain `/types` and numeric N×3 frame datasets at the root. CNDB v2
adds `/Header` attributes and optional `/_index`, and supports root frames or
root child groups containing `types` and `spatial_position/<frame>`. Group names
are arbitrary; no replica/chr naming convention is required. Both numeric names
and `t_<digits>` are sorted by their unsigned 64-bit ID. Ambiguous duplicate IDs
such as `2` and `t_2` are rejected. One group is loaded per molecule.

Root `/types` or selected group `types` must have length N. Fixed and variable
UTF-8 strings, signed/unsigned integers up to 64 bits, and finite numeric labels
are supported. Numeric labels are never mapped to amino acids or chromatin
compartments. Raw labels and their kind are retained separately from a labels TSV
override. Safe ASCII labels ≤15 bytes remain unchanged. Other labels receive
unique ASCII tokens after all already-valid tokens have been reserved.

Coordinates must be float32 or float64, with identical N across selected frames.
Every loaded value must be finite and fit VMD's float range after the optional
box-orientation rotation. Lengths are never rescaled.
HDF5 reads one frame at a time and uses gzip/zlib, shuffle, and Fletcher32.
SZIP, LZF, and custom codecs are not bundled. Local IO uses HDF5 directly rather
than interpreting the remote byte index.

All lengths remain in the trajectory's native unit, including coordinates,
periodic boxes, default radius (0.5), and type-specific radii. Optional Header
`coordinate_units` is an informational label; arbitrary unit names, including
reduced units, are preserved. Missing units are labeled `unspecified`. There is
no length conversion or inferred physical calibration. A declared
`format_version` must be `2.0`; a declared `format_name` must be `OpenMiChroM-CNDB`.

The per-frame numeric `box` attribute must be 3×3 vectors in coordinate units.
`-box` overrides it. Cell lengths and angles are computed from the Gram matrix.
For a general right-handed box, an orthonormal basis e1∥a, e2∥b−(b·e1)e1,
e3=e1×e2 transforms both box and coordinates into VMD's cell orientation. This
preserves distances and periodic geometry; input vectors and the transformation
are recorded. Degenerate and left-handed boxes are rejected.

chromdyn topology is read from `topology/atoms` compound fields `name`, `res_idx`,
`chain_idx`, plus `chain_ids`, `res_names`, and optional zero-based `bonds`.
Explicit PSF overrides this structure. Without either source, consecutive beads
form a single chain; no bond is guessed from distance. An existing embedded
topology with an absent/empty bond dataset has no bonds. Bonds cannot be repeated,
self-connected, or out of range.

CNDB v2 does not standardize bonds or boxes. Existing chromdyn extensions and
explicit input provide these. `genomic_position` is an N×2 signed 64-bit array.
Frame attributes are retained. `time_ps` sets VMD physical time directly;
`time` is also used when `time_units` is ps/picosecond or ns/nanosecond (frame
attribute first, Header fallback). Otherwise raw time remains in metadata and
VMD physical time is zero. Time-dependent particle labels are outside v1.
