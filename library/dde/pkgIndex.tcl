if {![package vsatisfies [package provide Tcl] 8.5-]} return
if {[info sharedlibextension] != ".dll"} return
package ifneeded dde 1.4.3 [list load [file join $dir tcldde14.dll] Dde]
