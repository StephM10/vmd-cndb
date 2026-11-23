# Manual loading of the extracted ZIP; installed VMD uses init.d + pkgIndex.
set vmdcndb_bootstrap_dir [file dirname [file normalize [info script]]]
if {$vmdcndb_bootstrap_dir ni $::auto_path} {lappend ::auto_path $vmdcndb_bootstrap_dir}
source -encoding utf-8 [file join $vmdcndb_bootstrap_dir register.tcl]
unset vmdcndb_bootstrap_dir
