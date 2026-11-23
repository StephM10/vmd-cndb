# Called by the stock VMD loadplugins.tcl init.d extension hook.
package require vmdcndb 0.2.1
if {![info exists ::vmdcndb::menu_registered]} {
    vmd_install_extension vmdcndb ::vmdcndb::gui {CNDB Loader}
    set ::vmdcndb::menu_registered 1
}
