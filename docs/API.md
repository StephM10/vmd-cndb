# Tcl API

Load `tcl/bootstrap.tcl` once, or use the installer. Namespace: `::vmdcndb`.

| Command | Result |
| --- | --- |
| `inspect FILE` | List of dictionaries: `name`, `beads`, `frames` |
| `load FILE ?-option value ...?` | New molecule ID after all selected frames load |
| `info MOLID` | Metadata for that molecule |
| `selection MOLID TYPE` | Safe VMD selection expression for the full label |
| `style MOLID TYPE ?-color NAME? ?-radius VALUE?` | Modify the type in that molecule, in trajectory units |

`load` options: `-trajectory` (required when several exist), `-first 0`,
`-last -1` (inclusive final position), `-stride 1`, `-labels {}`, `-psf {}`,
`-radius 0.5`, `-box {}`. First/last use sorted ordinal positions, not stored
frame IDs. `-box` contains nine row-major vector components in input units;
it overrides file boxes for all loaded frames.

Loading is synchronous. A failed coordinate read deletes the partial molecule
and reports an error; molfile itself conflates EOF and error, so the wrapper
also checks the read count. `_prepare` and `_finish` are private transaction
commands; applications should use `load`. Nested loads are rejected. Once VMD
has copied the selected frames, no HDF5 handle remains open.

`info` returns:

- `path`, `trajectory`, `layout`, `beads`, `total_frames`, `units`, `default_radius`.
- `raw_types`, `type_kind` (source HDF5 kind), `types`, `aliases`, `type_aliases`, `type_source`.
- `atoms` (original names, residue names/IDs, full chain labels), `bonds` (one-based pairs), `topology_source`.
- `genomic_position` (exact signed 64-bit interval endpoints, if present).
- `header`, and `frames`, with original frame `id` and HDF5 `path`, attributes,
  `box_vectors` (native source lengths), canonical VMD `cell`, `rotation`, and `time_ps`.
- `representations`: full type → stable VMD representation name.
- `type_styles`: full type → applied `color` and `radius` (trajectory units).

Length values always remain in the source trajectory unit. `units` is the Header's
label or `unspecified`; it does not select a conversion. V0.2 removes `-units`,
`scale_to_angstrom`, and `box_angstrom`. Migrate to the native values and
`box_vectors`; remove `-units` from scripts. A legacy `-units` argument produces
an explicit migration error. Both default and type-specific radii use the same
validation and native length convention as the molecule. See [length units](LENGTH_UNITS.md).

Attributes are dictionaries with `kind`, `shape`, and `values`. Values are
lossless text for integers and UTF-8 strings, and round-trip decimal text for
floating point. This avoids silently converting large integers to doubles.
An empty `shape` identifies a scalar. Unsupported opaque/compound attributes
raise an error; the opaque `/_index` dataset is deliberately ignored for local IO.

Colors use VMD's named palette, scoped through the molecule's representations.
Changing radii updates atoms of that type in that molecule. Native VMD selections
use the alias field (`type A1`, for example); `selection` resolves arbitrary full
labels safely. If a user deletes a type's representation manually, `style -color`
reports it; reload to reconstruct the default display.

Closing a molecule removes its metadata via VMD's molecule callback. `info` on
closed molecules raises an error. Full chain labels are preserved in metadata;
native `segid` uses unique aliases. Single-character chain labels are retained.
For additional long chain labels, a free one-character alias is used while
available; after 62 native chain identifiers are exhausted, those atoms have an
empty `chain` field and remain distinguishable by `segid` and original metadata.
