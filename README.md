# vmd-cndb

Open CNDB simulation trajectories directly in **Windows VMD**. No XTC conversion,
Python environment, or Conda installation is needed to use the prebuilt plugin.

**Early release: v0.1.0.** Native C++/HDF5 reader, Tcl 8.6 extension, and an
**Extensions → CNDB Loader** window. Tested with the installed VMD 2.0 Windows
alpha (banner 2.0.0a7; internal version 2.0.0a6), molfile ABI 18, x64.

## Install

1. Install a licensed Windows x64 VMD with Tcl 8.6.
2. Download and extract the [Windows ZIP](https://github.com/StephM10/vmd-cndb/releases/latest).
3. Close VMD. Open PowerShell in the extracted directory and run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\install.ps1
   ```

   The default VMD path is `C:\Program Files\University of Illinois\VMD2`.
   Supply `-VmdDir 'D:\path\to\VMD'` if needed. The default installation uses
   your personal startup file and requires no administrator rights.
4. Open **VMD CNDB Loader** from Windows Start and choose **Extensions → CNDB Loader**.
5. Browse to `examples/demo.cndb`, click **Inspect**, then **Load as new molecule**.

The installer copies the package to `%LOCALAPPDATA%\VMD-CNDB\0.1.0`. It appends a
marked bootstrap block to your personal `vmd.rc`, preserving its commands and
timestamped backups. A new personal file sources the installed defaults. If you launch
VMD from a directory containing another `vmd.rc`, run the installer with
`-StartupFile 'C:\that\directory\vmd.rc'` as well. See [installation](docs/INSTALL.md)
for manual loading, removal, and configuration details.

## Supported files

| Producer/layout | Coordinates | Structure |
| --- | --- | --- |
| Legacy OpenMiChroM | Root numeric datasets, including frame 0 | Root `types`; default linear chain |
| chromdyn | Root numeric datasets; per-frame `box` attribute | `types`, embedded atoms/chains/bonds |
| CNDB v2 root | `/Header`, `/types`, numeric frame datasets | Header units and available metadata |
| CNDB v2 multi-trajectory | `/<name>/spatial_position/<id>` | Select one named trajectory per molecule |

Float32, float64, gzip, shuffle, and Fletcher32 are supported. Numeric frame IDs,
including `t_0`, are sorted numerically. The GUI's first/last fields refer to
**zero-based positions in that sorted list**; last `-1` means the final position.

V1 reads local, fixed-particle-count simulation trajectories with complete finite
coordinates. Remote streaming and imaging data with missing loci are outside v1.
VMD retains loaded frames in memory; select a range and stride for large files.

## Arbitrary particle types

Types are category labels. Numeric labels retain their numbers; string labels
retain their text. Beads use generic structure fields, with no amino-acid mapping.
Choose a type in the loader to set its color and radius (Å).

Override types for a particular load with a UTF-8 `labels.tsv`:

```text
bead_id	type
1	active domain
2	my_custom_label
3	42
```

The separator is a tab. IDs start at **1** and must cover every bead exactly once.
The CNDB is always opened read-only. Full labels, original labels, and short VMD
aliases are available through `vmdcndb::info`. Long labels and labels with spaces
receive collision-free aliases. Native VMD's Type coloring uses only a label's
first character, so this plugin creates one sphere representation per complete
type and a separate bond representation. A1 and A2 can have independent colors.

## Tcl usage

```tcl
vmdcndb::inspect {C:/data/my trajectory.cndb}
set m [vmdcndb::load {C:/data/my trajectory.cndb} -first 0 -last -1 -stride 5]
set metadata [vmdcndb::info $m]
dict get $metadata type_aliases
set active [atomselect $m [vmdcndb::selection $m {active domain}]]
vmdcndb::style $m {active domain} -color orange -radius 4.0
$active delete
```

For multi-trajectory files, pass `-trajectory {/replica name}` from `inspect`.
Optional inputs: `-labels FILE`, `-psf FILE`, `-units auto|nm|angstrom`, and
`-box {ax ay az bx by bz cx cy cz}` in input coordinate units. Topology priority:
**explicit PSF → embedded topology → one linear chain**. Embedded topology with
no bonds remains unbonded. PSF files must use sequential one-based atom IDs and
integer residue IDs. Bead order must match the CNDB.

Coordinates and boxes are converted to Å. Old files default to nm; v2 reads
`Header.coordinate_units`. Boxes with rotated axes are transformed to VMD's
canonical cell orientation, including the coordinates; original vectors and
rotation matrices remain in the metadata. See [format details](docs/FORMAT.md)
and [API](docs/API.md).

## Build and test

Windows x64, Visual Studio 2022 with C++ tools, CMake ≥3.24, and an internet
connection for pinned source archives:

```powershell
.\scripts\build.ps1 -Package
python -m pip install -r tests/requirements.txt
python tests/test_reader.py --exe build/plugin/Release/cndb_check.exe
python tests/test_installer.py --package dist/vmd-cndb-0.1.0-windows-x64
```

HDF5 1.14.6, zlib 1.3.1, the Microsoft C/C++ runtime, and the Tcl stub library are
linked statically. The plugin depends on Windows and the Tcl runtime supplied by
VMD. Dependency sources are verified by SHA256. Windows CI builds the ZIP and
runs the reader and installer tests without redistributing VMD.

For a real VMD integration test:

```powershell
python tests/run_vmd.py --vmd 'C:/Program Files/University of Illinois/VMD2/vmd.exe' --package dist/vmd-cndb-0.1.0-windows-x64 --log build/vmd-test.log
```

Add `--gui` to exercise the Tk window. After installation, add `--normal-startup`
and `--startup-file 'C:/Users/your-name/vmd.rc'` to test the installed launcher's
automatic initialization. [Validation record](docs/VALIDATION.md).

## Origin and license

Independent reader for the CNDB formats produced by
[OpenMiChroM](https://github.com/junioreif/OpenMiChroM) and
[chromdyn](https://github.com/s-brahmachari/chromdyn). CNDB v2 behavior is based on
the [pinned upstream specification](https://github.com/junioreif/OpenMiChroM/blob/b7498f2589410b582e94befd779b0749f87a0c1a/docs/source/Developer/structural_file_formats.rst).

New code is [MIT licensed](LICENSE). VMD SDK headers retain UIUC notices; HDF5,
zlib, and Tcl retain their licenses in `third_party/licenses` and the binary
package. VMD itself is not included. This project is not an official VMD plugin.
See [third-party notices](docs/THIRD_PARTY.md).

GPU rendering and a general Python VMD controller remain future work.
