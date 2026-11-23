# One length unit per trajectory

V0.2 preserves the trajectory's native length unit throughout loading and styling:

| Quantity | Convention |
| --- | --- |
| Coordinates | Source lengths, with only the existing box-orientation rotation if required |
| Periodic box and supplied box override | Source lengths |
| Initial bead radius | 0.5 in source units, configurable with `load -radius VALUE` or the GUI |
| Type radius | Source units, configurable with `style -radius VALUE` or the GUI |
| Bond cylinder radius | 0.1 in source units (display setting) |

For reduced coordinates with an unstretched bond length of 1 and bead radius of
0.5, VMD receives length 1 and radius 0.5. The loader preserves instantaneous bond
stretching and fluctuations. It does not normalize individual bonds to 1 or
infer bead radii from particle labels or residue names.

`Header.coordinate_units`, if present, is recorded in `info units`. It does not
change any length. Missing metadata is recorded as `unspecified`. The loader
does not infer a physical length scale from the `.cndb` extension or an OpenMM
writer's choice of numeric storage units.

VMD itself uses Å terminology internally. Its built-in distance labels and some
exports may consequently say Å even for reduced coordinates. Their numbers are
in this molecule's source unit. Physical calibration and comparison of files
stored in different units require an explicit external conversion of all lengths.
The CNDB Loader reports native units and provides no conversion selector.

## Migration from v0.1

V0.1 assumed that files without a Header used nm, multiplied their coordinates
and boxes by 10, and used an independent default radius of 1.5 Å. A user-entered
radius of 0.5 stayed at 0.5, so a raw bond length of 1 appeared as length 10 and
radius/bond became 0.05. This was a length-convention bug.

V0.2 removes the conversion function and `-units`/`--units` option, and replaces
`box_angstrom` with `box_vectors`. The obsolete `scale_to_angstrom` metadata is
removed. Script callers should remove `-units` and specify radii directly in
trajectory units. The GUI exposes a global default radius and remembers each
type's applied radius when switching types.

Restart VMD after installing v0.2 and reload the original CNDB. Molecules already
loaded by v0.1 retain their previous coordinates and styles in memory.
