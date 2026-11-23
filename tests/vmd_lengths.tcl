proc assert {condition message} {if {![uplevel 1 [list expr $condition]]} {error $message}}
proc lengths_test {} {
    source -encoding utf-8 [file join $::env(CNDB_PACKAGE) tcl bootstrap.tcl]
    foreach name {legacy nm angstrom reduced custom missing} {
        set path [file join $::env(CNDB_LENGTH_FIXTURES) $name.cndb]
        set m [vmdcndb::load $path]
        set s [atomselect $m all]
        set x [$s get x]
        set bond [expr {[lindex $x 1]-[lindex $x 0]}]
        assert {abs($bond-1.0)<1e-6} "$name: source bond length 1 was rescaled to $bond"
        foreach r [$s get radius] {assert {abs($r/$bond-0.5)<1e-6} "$name: default radius/bond must be 0.5, got [expr {$r/$bond}]"}
        assert {abs([lindex [molinfo $m get {a b c}] 0]-8)<1e-6} "$name: box was rescaled"
        vmdcndb::style $m A -radius 0.25
        set radii [$s get radius]
        assert {abs([lindex $radii 0]-0.25)<1e-6 && abs([lindex $radii 1]-0.5)<1e-6} "$name: type-specific radius changed units or other types"
        set styles [dict get [vmdcndb::info $m] type_styles]
        assert {[dict get $styles A radius] == 0.25 && [dict get $styles B radius] == 0.5} {style provenance lost}
        $s delete
        mol delete $m
        set m [vmdcndb::load $path -radius 0.8]
        set s [atomselect $m all]
        foreach r [$s get radius] {assert {abs($r-0.8)<1e-6} "$name: global radius override failed"}
        $s delete
        mol delete $m
    }
    set path [file join $::env(CNDB_LENGTH_FIXTURES) legacy.cndb]
    foreach bad {0 -1 NaN Inf nope 1e39 1e-50} {
        set before [molinfo list]
        assert {[catch {vmdcndb::load $path -radius $bad} message]} "invalid radius $bad was accepted"
        assert {[molinfo list] eq $before} {invalid radius created a partial molecule}
    }
    set m [vmdcndb::load $path]
    set s [atomselect $m all]
    foreach bad {0 -1 NaN Inf nope 1e39 1e-50} {
        assert {[catch {vmdcndb::style $m A -radius $bad} message]} "invalid type radius $bad was accepted"
        foreach r [$s get radius] {assert {$r == 0.5} {invalid type radius changed atoms}}
        assert {[dict get [vmdcndb::info $m] type_styles A radius] == 0.5} {invalid type radius changed metadata}
    }
    $s delete
    mol delete $m
    assert {[catch {vmdcndb::load $path -units nm} message]} {removed unit conversion option was accepted}
    assert {[string match *preserv* $message]} {migration error did not explain native units}
    puts CNDB_LENGTHS_OK
}
if {[catch {lengths_test} message opts]} {puts stderr "CNDB_LENGTHS_FAILED: $message\n[dict get $opts -errorinfo]"}
quit
