if {![package vsatisfies [package provide Tcl] 8.5]} return
if {[info sharedlibextension] ne ".dll"} return
if {[::tcl::pkgconfig get debug]} {
    package ifneeded dde 1.4.0b1 [list load [file join $dir tcldde14g.dll] dde]
} else {
    package ifneeded dde 1.4.0b1 [list load [file join $dir tcldde14.dll] dde]
}
