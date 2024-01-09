#!/usr/bin/env tclsh
if {$argc < 1} {
    error "need a filename argument"
}
lassign $argv filename
set f [open $filename a]
try {
    puts $f "TCL_VERSION=[info tclversion]"
    puts $f "TCL_PATCHLEVEL=[info patchlevel]"
} finally {
    close $f
}
