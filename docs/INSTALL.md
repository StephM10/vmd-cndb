# Windows installation

The ZIP is self-contained apart from VMD's Tcl 8.6 runtime and Windows system
libraries. The native plugin has no Python, Conda, HDF5 DLL, or VC runtime DLL
dependency. It targets molfile ABI 18 and x64; other VMD builds require testing.

Close VMD and run the extracted `install.ps1`. `-VmdDir` selects the VMD
installation. `-Destination` changes the private plugin directory. It must be
empty or already managed by this installer; keep it separate from the ZIP.

The default installation uses `HOME` (or `USERPROFILE`) `vmd.rc` and creates a
**VMD CNDB Loader** Start menu shortcut that selects it explicitly. No system
directory changes or administrator rights are needed. A new personal file
sources VMD's installed defaults; an existing personal file keeps its contents.
It receives an ASCII, marked bootstrap block; existing
commands are retained, and original bytes are backed up beside the file. The
bootstrap sources an absolute UTF-8 path encoded as hex to avoid Tcl quoting or
Windows encoding problems. Repeated installation replaces that block once.

VMD can use a `vmd.rc` in its launch directory instead of personal defaults. Supply
an existing effective file with `-StartupFile 'C:\path\vmd.rc'` to target it
explicitly. This also supports user-only configuration when an existing personal
startup file takes precedence. The install-state manifest records touched files
and backups. Re-running for another startup file retains earlier records.

To explicitly update a system `vmd.rc` under Program Files, use an administrator
PowerShell window and `-StartupFile`. Do not replace an existing
startup file with a blank one; that can suppress normal VMD defaults.

To load manually for the current session, use VMD's Tk Console:

```tcl
source {C:/path/to/extracted-package/tcl/bootstrap.tcl}
```

To remove integration, run the installed `uninstall.ps1`. It removes only the
marked blocks and preserves subsequent user edits, installed files, and backups.
Restart VMD. Restore an original backup manually only if you want to discard all
later changes to that configuration. The Start menu shortcut is retained as a
normal VMD launcher after removal; the personal file still sources VMD defaults.

If the extension fails to appear, inspect VMD's text console for `CNDB Loader:`.
Check that it is a Windows x64 Tcl 8.6 build and that VMD used the expected
startup file. `package present vmdcndb` should return `0.1.0`; `vmdcndb::inspect`
should list the example trajectory. A fresh VMD process is required after
replacing the loaded DLL.
