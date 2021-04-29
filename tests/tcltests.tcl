#! /usr/bin/env tclsh

package require tcltest 2.5
namespace import ::tcltest::*
testConstraint exec          [llength [info commands exec]]
testConstraint debug [expr {"debug" in [split [package provide tcl] .]}]
testConstraint purify [expr {"purify" in [split [package provide tcl] .]}]
testConstraint debugpurify [
    expr {
	"memdebug" ni [split [package provide tcl] .]
	&& [testConstraint debug]
	&& [testConstraint purify]
    }]
testConstraint fcopy         [llength [info commands fcopy]]
testConstraint fileevent     [llength [info commands fileevent]]
testConstraint thread        [expr {![catch {package require Thread 2.7-}]}]
testConstraint notValgrind   [expr {![testConstraint valgrind]}]


namespace eval ::tcltests {


    proc init {} {
	if {[namespace which ::tcl::file::tempdir] eq {}} {
	    interp alias {} [namespace current]::tempdir {} [
		namespace current]::tempdir_alternate
	} else {
	    interp alias {} [namespace current]::tempdir {} ::tcl::file::tempdir
	}
    }


    proc tempdir_alternate {} {
	close [file tempfile tempfile]
	set tmpdir [file dirname $tempfile]
	set execname [info nameofexecutable]
	regsub -all {[^[:alpha:][:digit:]]} $execname _ execname
	for {set i 0} {$i < 10000} {incr i} {
	    set time [clock milliseconds]
	    set name $tmpdir/${execname}_${time}_$i
	    if {![file exists $name]} {
		file mkdir $name
		return $name
	    }
	}
	error [list {could not create temporary directory}]
    }

    init

    package provide tcltests 0.1
}

