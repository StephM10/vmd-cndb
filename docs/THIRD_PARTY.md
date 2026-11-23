# Third-party components

| Component | Version/source | Use | License notice |
| --- | --- | --- | --- |
| VMD molfile SDK | ABI 18; local VMD distribution headers, 2019/2020 revisions | Plugin ABI declarations | `VMD-SDK.txt`, UIUC Open Source |
| HDF5 | 1.14.6, official HDF Group release | Static C IO library | `HDF5.txt` |
| zlib | 1.3.1, zlib.net fossil archive | Static gzip decompression | `zlib.txt` |
| Tcl | core-8-6-14, official tcltk/tcl | Public headers and static stub library | `Tcl.txt` / `third_party/tcl/license.terms` |

Binary packages include all notices in `licenses/`. New source in `src/`,
`tcl/`, `scripts/`, `tests/`, and `examples/` is MIT licensed. VMD SDK headers
and vendored Tcl headers retain their original copyrights. No OpenMiChroM,
chromdyn, H5MD reader, or VMD implementation source was copied into the reader.

VMD itself is separately distributed under its own license. Obtain it from the
[VMD developers](https://www.ks.uiuc.edu/Research/vmd/). Its public SDK plugin
license is published [here](https://www.ks.uiuc.edu/Research/vmd/plugins/pluginlicense.html).
Use VMD's requested scientific citation when publishing figures made with VMD.
