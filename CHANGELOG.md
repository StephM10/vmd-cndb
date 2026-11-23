# Changelog

## 0.2.1 — 2026-11-23

- Install in VMD standard native/Tcl plugin directories and its init.d hook.
- Use pkgIndex/package require and preserve the original executable/startup files.
- Remove the requirement for a custom startup-file shortcut.
- Back up and migrate the old managed personal startup block.
- Verify ordinary startup in an isolated copy of the stock VMD installation.

## 0.2.0 — 2026-11-23

- Fix the length-convention bug: preserve native coordinate and box lengths.
- Use the same trajectory unit for global and type-specific radii; default 0.5.
- Remove automatic unit conversion and its GUI selector and `-units` API option.
- Retain Header units as metadata; use `unspecified` when absent.
- Replace `box_angstrom` with `box_vectors` and remove `scale_to_angstrom`.
- Add native-unit and radius regression coverage in the reader and real VMD.
- Add a global radius input and per-type radius/color readback in the GUI.
- Correct the Windows VMD 2 menu documentation to Plugins → CNDB Loader.

## 0.1.0 — 2026-11-22

- Native Windows x64 CNDB reader for legacy OpenMiChroM, chromdyn, and CNDB v2.
- VMD loader window and Tcl inspect/load/info API, with frame selection.
- Arbitrary types, UTF-8 external labels, collision-free aliases, per-type styles.
- PSF/embedded/linear topology, unit conversion, periodic cells, source metadata.
- Static HDF5/zlib/runtime packaging, reversible installer, Windows CI and tests.
