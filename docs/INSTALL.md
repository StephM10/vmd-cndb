# Standard Windows VMD plugin installation

The installer uses VMD's own plugin search and registration mechanisms:

| Component | Path relative to the original VMD directory |
| --- | --- |
| Native molfile reader | `plugins/WIN64/molfile/vmdcndb.so` |
| Tcl package | `plugins/noarch/tcl/vmdcndb0.2.1/` |
| GUI registration hook | `scripts/init.d/vmdcndb.tcl` |

The `.so` file is a Windows x64 PE DLL, matching the naming convention of VMD's
other WIN64 molfile plugins. The package uses `pkgIndex.tcl`, `package require`,
and `vmd_install_extension`. VMD's shipped `loadplugins.tcl` scans the native
reader directory and sources `scripts/init.d` registrations for graphical mode.
The installer does not replace `vmd.exe`, `vmd.rc`, or VMD's built-in scripts.
No custom executable, launcher, or `-startup` argument is required.

## Install

Close VMD. Extract the Windows ZIP and run `install.ps1` in an administrator
PowerShell when VMD is installed under Program Files:

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

Use `-VmdDir 'D:/path/to/VMD'` if needed. The default is
`C:/Program Files/University of Illinois/VMD2`. The installer checks write access
before copying plugin files. Existing unmanaged plugin files are preserved.

Open the original VMD normally, then choose **Plugins → CNDB Loader**. In text
mode the reader is discovered automatically; use `package require vmdcndb` to
load its Tcl inspect/load/info commands explicitly.

The plugin is self-contained apart from Windows and VMD's Tcl 8.6 runtime.
It targets x64 molfile ABI 18. VMD itself is not included.

## Migration from v0.2.0 and earlier

Earlier installers stored files under `%LOCALAPPDATA%/VMD-CNDB` and used a marked
block in a personal `vmd.rc`. They also created a shortcut that selected that file
with `-startup`; it targeted the original VMD executable.

The standard installer backs up and removes that marked block so it cannot load
an older package before the standard package is discovered. Other personal
commands, including a command sourcing installed defaults, are retained. Supply
`-LegacyStartupFile 'C:/Users/your-name/vmd.rc'` when installing on behalf of a
different user. Old AppData files and the earlier shortcut are left available;
the original VMD entry point now works independently of them.

## Manual session loading and removal

For a temporary session without installing, source the extracted ZIP's
`tcl/bootstrap.tcl` from VMD. It uses the same Tcl package, then registers the menu:

```tcl
source {C:/path/to/extracted-package/tcl/bootstrap.tcl}
```

To remove automatic discovery, run the installed package's `uninstall.ps1` with
administrator rights if required. It validates the ownership manifest and checks
file hashes before removing its native reader, registration hook, and package
index. It preserves modified files, documentation, examples and migration records.
Restart VMD after installation or removal. Check `package present vmdcndb` after
opening the loader; it should return `0.2.1`.

## Evidence

The [official Tcl plugin guide](https://www.ks.uiuc.edu/Research/vmd/plugins/doxygen/tcltkplugins.html)
documents package indexes/search directories, `package require`, and menu
registration. The installed VMD `scripts/vmd/loadplugins.tcl` additionally exposes
the `scripts/init.d` hook. Its [developer's installation explanation](https://www.ks.uiuc.edu/Research/vmd/mailing_list/vmd-l/20951.html)
describes placing packages in the plugin directory and registering them with
`vmd_install_extension`. [Startup search order](https://www.ks.uiuc.edu/Research/vmd/current/ig/node8.html)
explains why a personal startup-file shortcut could behave differently from
opening VMD in its own installation directory.
