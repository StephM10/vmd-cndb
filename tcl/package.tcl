# Package initialization; menu registration belongs to VMD's init.d hook.
set vmdcndb_dir [file dirname [file normalize [info script]]]
set vmdcndb_library [file normalize [file join $vmdcndb_dir .. bin vmdcndb.dll]]
if {![file exists $vmdcndb_library]} {
    set vmdcndb_library [file join $env(VMDDIR) plugins [vmdinfo arch] molfile vmdcndb.so]
}
load $vmdcndb_library Vmdcndb
if {[lsearch -exact [plugin list {mol file reader}] [list {mol file reader} vmdcndb]] < 0} {
    plugin dlopen $vmdcndb_library
    plugin update
}
source -encoding utf-8 [file join $vmdcndb_dir cndb.tcl]
set ::vmdcndb::ready 1
unset vmdcndb_dir vmdcndb_library
