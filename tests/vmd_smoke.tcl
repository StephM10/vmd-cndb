# Run inside a licensed VMD installation, with CNDB_PACKAGE set to the ZIP root.
proc assert {condition message} {if {![uplevel 1 [list expr $condition]]} {error $message}}
proc run_smoke {} {
    set root $::env(CNDB_PACKAGE)
    if {[info exists ::env(CNDB_REQUIRE_AUTOSTART)]} {
        assert {[info exists ::vmdcndb::ready] && $::vmdcndb::ready} {ordinary VMD startup did not load CNDB Loader}
    }
    source [file join $root tcl bootstrap.tcl]
    source [file join $root tcl bootstrap.tcl]
    set example [file join $root examples demo.cndb]
    set choices [::vmdcndb::inspect $example]
    assert {[llength $choices] == 1} {inspect failed}
    set id [::vmdcndb::load $example -first 0 -last 18 -stride 3]
    assert {[molinfo $id get numatoms] == 32} {atom count failed}
    assert {[molinfo $id get numframes] == 7} {selected frame count failed}
    set meta [::vmdcndb::info $id]
    assert {[dict get $meta trajectory] eq {/}} {metadata failed}
    assert {[lmap f [dict get $meta frames] {dict get $f id}] eq {0 3 6 9 12 15 18}} {frame IDs failed}
    set sel [atomselect $id all frame 0]
    set xyz [$sel get {x y z}]
    assert {abs([lindex $xyz 0 0] - 2) < 1e-5 && abs([lindex $xyz 0 2] + 3) < 1e-5} {native coordinates changed}
    assert {[lindex [$sel get type] 16] eq {B1}} {short type was altered}
    set bonds [$sel getbonds]
    assert {[lindex $bonds 0] eq {1} && [lindex $bonds 31] eq {30}} {linear bonds failed}
    assert {[llength [lindex $bonds 15]] == 2} {unexpected distance-inferred bonds}
    assert {[molinfo $id get {a b c}] eq {12.000000 12.000000 12.000000} || abs([lindex [molinfo $id get {a b c}] 0]-12) < 1e-5} {periodic box failed}
    set a [atomselect $id [::vmdcndb::selection $id {active domain}]]
    assert {[$a num] == 16} {long type selection failed}
    ::vmdcndb::style $id {active domain} -color orange -radius 4.2
    assert {abs([lindex [$a get radius] 0] - 4.2) < 1e-5} {type radius failed}
    set rep [mol repindex $id [dict get $meta representations {active domain}]]
    set coloring [lindex [molinfo $id get [list [list color $rep]]] 0]
    assert {$coloring eq "ColorID [colorinfo index orange]"} {type color failed}
    animate goto 6
    assert {[molinfo $id get frame] == 6} {animation frame failed}
    $a delete
    $sel delete
    mol delete $id
    assert {[catch {::vmdcndb::info $id}]} {closed molecule metadata should fail}
    for {set i 0} {$i < 4} {incr i} {
        set id [::vmdcndb::load $example -labels [file join $root examples labels.tsv]]
        set a [atomselect $id [::vmdcndb::selection $id {custom active type}]]
        assert {[$a num] == 16} {external labels failed}
        $a delete
        mol delete $id
    }
    assert {[dict size $::vmdcndb::records] == 0} {closed molecules retained metadata}
    if {[info exists ::env(CNDB_EXTRA_SMOKE)]} {source -encoding utf-8 $::env(CNDB_EXTRA_SMOKE)}
    # A GUI run additionally constructs the window and checks the extension menu.
    if {[llength [info commands winfo]]} {
        menu vmdcndb on
        set w .vmdcndb
        assert {[winfo exists $w] && [winfo exists .vmdcndb.trajectory]} {loader window missing}
        assert {![winfo exists .vmdcndb.eunits]} {obsolete unit conversion control remains}
        set ::vmdcndb::ui(file) $example
        set ::vmdcndb::ui(first) 0
        set ::vmdcndb::ui(last) 0
        set ::vmdcndb::ui(default_radius) 0.7
        ::vmdcndb::gui_inspect
        ::vmdcndb::gui_load
        set m $::vmdcndb::ui(molecule)
        set s [atomselect $m all]
        foreach r [$s get radius] {assert {abs($r-0.7)<1e-6} {GUI default radius failed}}
        ::vmdcndb::style $m {active domain} -radius 0.25
        set ::vmdcndb::ui(type) {active domain}
        event generate .vmdcndb.types <<ComboboxSelected>>
        assert {$::vmdcndb::ui(radius) == 0.25} {type radius readback failed}
        set ::vmdcndb::ui(type) B1
        event generate .vmdcndb.types <<ComboboxSelected>>
        assert {$::vmdcndb::ui(radius) == 0.7} {type radius readback leaked from another type}
        $s delete
        mol delete $m
        puts {CNDB_GUI_OK}
        destroy $w
    }
    puts {CNDB_SMOKE_OK}
}
if {[catch {run_smoke} message opts]} {
    puts stderr "CNDB_SMOKE_FAILED: $message\n[dict get $opts -errorinfo]"
} else {puts {All VMD integration checks passed.}}
quit
