# Public entry points: inspect FILE; load FILE ?options?; info MOLID.
namespace eval ::vmdcndb {
    variable records [dict create]
    variable ui
    array set ui {file {} trajectory {} first 0 last -1 stride 1 labels {} psf {} box {} status {} molecule {} type {} color red radius 0.5 default_radius 0.5 unit_text {Lengths: native trajectory units}}
}
proc ::vmdcndb::_molecule_changed {name id operation} {
    variable records
    if {[::info exists ::vmd_molecule($id)] && $::vmd_molecule($id) == 0 && [dict exists $records $id]} {
        dict unset records $id
    }
}
trace add variable ::vmd_molecule write ::vmdcndb::_molecule_changed

proc ::vmdcndb::load {path args} {
    variable records
    set options [dict create trajectory {} first 0 last -1 stride 1 labels {} psf {} radius 0.5 box {}]
    if {[llength $args] % 2} { error {CNDB: options require -name value pairs} }
    foreach {key value} $args {
        if {$key eq {-units}} {error {CNDB: -units was removed; v0.2 preserves trajectory length units}}
        if {![string match -* $key] || ![dict exists $options [string range $key 1 end]]} {
            error "CNDB: unknown option $key"
        }
        dict set options [string range $key 1 end] $value
    }
    set path [file normalize $path]
    set meta [_prepare $path $options]
    set id {}
    set outcome {}
    set failure {}
    try {
        set id [mol new $path type vmdcndb first 0 last -1 step 1 waitfor all autobonds off]
    } on error {message opts} {
        set failure $message
    } finally {
        set outcome [_finish]
    }
    if {$failure ne {}} {error "CNDB: $failure; [dict get $outcome error]"}
    set expected [llength [dict get $meta frames]]
    if {[dict get $outcome error] ne {} || [dict get $outcome frames_read] != $expected || [molinfo $id get numframes] != $expected} {
        mol delete $id
        error "CNDB: load failed: [dict get $outcome error] (read [dict get $outcome frames_read]/$expected frames)"
    }
    # Keep provenance with the molecule rather than the last filename loaded.
    dict set records $id $meta
    mol rename $id "CNDB: [file tail $path] [dict get $meta trajectory]"
    mol modcolor 0 $id ColorID 8
    mol modstyle 0 $id Bonds 0.1 12
    set palette {red blue green orange yellow cyan purple pink tan silver lime magenta}
    set n 0
    dict for {type alias} [dict get $meta type_aliases] {
        set color [lindex $palette [expr {$n % [llength $palette]}]]
        # VMD's built-in Type coloring uses only the first character. Give
        # each complete label a representation so A1 and A2 stay independent.
        mol addrep $id
        set rep [expr {[molinfo $id get numreps]-1}]
        mol modselect $rep $id "type $alias"
        mol modstyle $rep $id VDW 1.0 12
        mol modcolor $rep $id ColorID [colorinfo index $color]
        dict set meta representations $type [mol repname $id $rep]
        dict set meta type_styles $type [dict create color $color radius [dict get $meta default_radius]]
        incr n
    }
    dict set records $id $meta
    display resetview
    return $id
}

proc ::vmdcndb::info {id} {
    variable records
    if {[catch {molinfo $id get numatoms}] || ![dict exists $records $id]} {
        if {[dict exists $records $id]} {dict unset records $id}
        error "CNDB: molecule $id was closed or was not loaded with vmdcndb::load"
    }
    return [dict get $records $id]
}

proc ::vmdcndb::selection {id type} {
    set meta [info $id]
    if {![dict exists $meta type_aliases $type]} {error "CNDB: unknown type '$type'"}
    return "type [dict get $meta type_aliases $type]"
}

proc ::vmdcndb::style {id type args} {
    set expr [selection $id $type]
    if {[llength $args] % 2} {error {CNDB: style requires -color NAME and/or -radius VALUE_IN_TRAJECTORY_UNITS}}
    foreach {key value} $args {
        switch -- $key {
            -color {
                if {$value ni [colorinfo colors]} {error "CNDB: unknown VMD color '$value'"}
            }
            -radius {
                _validate_radius $value
            }
            default {error "CNDB: unknown style option $key"}
        }
    }
    set sel [atomselect $id $expr]
    try {
        foreach {key value} $args {
            if {$key eq {-color}} {
                set rep [mol repindex $id [dict get [info $id] representations $type]]
                if {$rep < 0} {error {CNDB: the type representation was removed in VMD; reload the molecule to restore it}}
                mol modcolor $rep $id ColorID [colorinfo index $value]
            } else {$sel set radius $value}
            dict set ::vmdcndb::records $id type_styles $type [string range $key 1 end] $value
        }
    } finally {$sel delete}
    return
}

proc ::vmdcndb::browse {key} {
    variable ui
    set patterns [expr {$key eq "file" ? {{{CNDB files} {.cndb .h5 .hdf5}} {{All files} *}} : {{{All files} *}}}]
    set file [tk_getOpenFile -filetypes $patterns]
    if {$file ne {}} {
        set ui($key) $file
        if {$key eq "file"} {gui_inspect}
    }
}

proc ::vmdcndb::gui_inspect {} {
    variable ui
    try {
        set choices [inspect $ui(file)]
        set names {};foreach choice $choices {lappend names [dict get $choice name]}
        .vmdcndb.trajectory configure -values $names
        set ui(trajectory) [lindex $names 0]
        set ui(status) "[llength $choices] trajectory/trajectories. Select frames by sorted position (0-based)."
    } on error {message opts} {set ui(status) $message}
}

proc ::vmdcndb::gui_load {} {
    variable ui
    set ui(status) {Loading selected frames...}
    update idletasks
    try {
        set id [load $ui(file) -trajectory $ui(trajectory) -first $ui(first) -last $ui(last) -stride $ui(stride) -labels $ui(labels) -psf $ui(psf) -radius $ui(default_radius) -box $ui(box)]
        set ui(molecule) $id
        set types [dict keys [dict get [info $id] type_aliases]]
        .vmdcndb.types configure -values $types
        set ui(type) [lindex $types 0]
        gui_type_changed
        set ui(unit_text) "Lengths: [dict get [info $id] units] (native trajectory values)"
        set ui(status) "Loaded molecule $id: [molinfo $id get numatoms] beads, [molinfo $id get numframes] frames."
    } on error {message opts} {set ui(status) $message;tk_messageBox -icon error -title {CNDB Loader} -message $message}
}

proc ::vmdcndb::gui_type_changed {} {
    variable ui
    set current [dict get [info $ui(molecule)] type_styles $ui(type)]
    set ui(radius) [dict get $current radius]
    set ui(color) [dict get $current color]
}

proc ::vmdcndb::gui_style {} {
    variable ui
    try {
        style $ui(molecule) $ui(type) -color $ui(color) -radius $ui(radius)
        set ui(status) "Updated type $ui(type) in molecule $ui(molecule)."
    } on error {message opts} {set ui(status) $message}
}

proc ::vmdcndb::gui {} {
    variable ui
    if {[winfo exists .vmdcndb]} {wm deiconify .vmdcndb;raise .vmdcndb;return .vmdcndb}
    package require Tk
    toplevel .vmdcndb
    wm title .vmdcndb {CNDB Loader}
    wm protocol .vmdcndb WM_DELETE_WINDOW {wm withdraw .vmdcndb}
    set row 0
    foreach {key title} {file {CNDB file} labels {Labels TSV (optional)} psf {Topology PSF (optional)}} {
        ttk::label .vmdcndb.l$key -text $title
        ttk::entry .vmdcndb.e$key -textvariable ::vmdcndb::ui($key) -width 56
        ttk::button .vmdcndb.b$key -text Browse -command [list ::vmdcndb::browse $key]
        grid .vmdcndb.l$key -row $row -column 0 -sticky w -padx 8 -pady 4
        grid .vmdcndb.e$key -row $row -column 1 -sticky ew
        grid .vmdcndb.b$key -row $row -column 2 -padx 8
        incr row
    }
    ttk::label .vmdcndb.lt -text Trajectory
    ttk::combobox .vmdcndb.trajectory -textvariable ::vmdcndb::ui(trajectory) -state readonly
    ttk::button .vmdcndb.inspect -text Inspect -command ::vmdcndb::gui_inspect
    grid .vmdcndb.lt -row $row -column 0 -sticky w -padx 8
    grid .vmdcndb.trajectory -row $row -column 1 -sticky ew
    grid .vmdcndb.inspect -row $row -column 2 -padx 8
    incr row
    foreach {key title} {first {First position (0-based)} last {Last position (-1: final)} stride Stride default_radius {Default bead radius (trajectory units)} box {Box vectors (optional, 9 values)}} {
        ttk::label .vmdcndb.l$key -text $title
        ttk::entry .vmdcndb.e$key -textvariable ::vmdcndb::ui($key)
        grid .vmdcndb.l$key -row $row -column 0 -sticky w -padx 8 -pady 4
        grid .vmdcndb.e$key -row $row -column 1 -columnspan 2 -sticky ew -padx 8
        incr row
    }
    ttk::label .vmdcndb.units -textvariable ::vmdcndb::ui(unit_text)
    grid .vmdcndb.units -row $row -column 0 -columnspan 3 -sticky w -padx 8 -pady 4
    incr row
    ttk::button .vmdcndb.load -text {Load as new molecule} -command ::vmdcndb::gui_load
    grid .vmdcndb.load -row $row -column 0 -columnspan 3 -pady 8
    incr row
    ttk::label .vmdcndb.ltypes -text {Type style}
    ttk::combobox .vmdcndb.types -textvariable ::vmdcndb::ui(type) -state readonly
    bind .vmdcndb.types <<ComboboxSelected>> ::vmdcndb::gui_type_changed
    grid .vmdcndb.ltypes -row $row -column 0 -sticky w -padx 8
    grid .vmdcndb.types -row $row -column 1 -columnspan 2 -sticky ew -padx 8
    incr row
    ttk::label .vmdcndb.lcolor -text {Color / radius (trajectory units)}
    ttk::combobox .vmdcndb.color -textvariable ::vmdcndb::ui(color) -values [colorinfo colors] -state readonly
    ttk::entry .vmdcndb.radius -textvariable ::vmdcndb::ui(radius) -width 8
    grid .vmdcndb.lcolor -row $row -column 0 -sticky w -padx 8 -pady 4
    grid .vmdcndb.color -row $row -column 1 -sticky ew
    grid .vmdcndb.radius -row $row -column 2 -padx 8
    incr row
    ttk::button .vmdcndb.apply -text {Apply type style} -command ::vmdcndb::gui_style
    grid .vmdcndb.apply -row $row -column 0 -columnspan 3 -pady 6
    incr row
    ttk::label .vmdcndb.status -textvariable ::vmdcndb::ui(status) -wraplength 620
    grid .vmdcndb.status -row $row -column 0 -columnspan 3 -sticky ew -padx 8 -pady 8
    grid columnconfigure .vmdcndb 1 -weight 1
    return .vmdcndb
}
package provide vmdcndb 0.2.1
