# MIT, Copyright (c) 2026 Leren Wang. Safe to source repeatedly.
if {![namespace exists ::vmdcndb] || ![info exists ::vmdcndb::ready]} {
    set cndb_root [file dirname [file dirname [file normalize [info script]]]]
    load [file join $cndb_root bin vmdcndb.dll] Vmdcndb
    plugin dlopen [file join $cndb_root bin vmdcndb.dll]
    plugin update
    source -encoding utf-8 [file join $cndb_root tcl cndb.tcl]
    set ::vmdcndb::ready 1
    unset cndb_root
}
