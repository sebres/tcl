# safe.tcl --
#
# This file provide a safe loading/sourcing mechanism for safe interpreters.
# It implements a virtual path mecanism to hide the real pathnames from the
# slave. It runs in a master interpreter and sets up data structure and
# aliases that will be invoked when used from a slave interpreter.
#
# See the safe.n man page for details.
#
# Copyright (c) 1996-1997 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.

#
# The implementation is based on namespaces. These naming conventions are
# followed:
# Private procs starts with uppercase.
# Public  procs are exported and starts with lowercase
#

# Needed utilities package
package require opt 0.4.7

# Create the safe namespace
namespace eval ::safe {
    # Exported API:
    namespace export interpCreate interpInit interpConfigure interpDelete \
	interpAddToAccessPath interpFindInAccessPath setLogCmd
}

# Helper function to resolve the dual way of specifying staticsok (either
# by -noStatics or -statics 0)
proc ::safe::InterpStatics {} {
    foreach v {Args statics noStatics} {
	upvar $v $v
    }
    set flag [::tcl::OptProcArgGiven -noStatics]
    if {$flag && (!$noStatics == !$statics)
	&& ([::tcl::OptProcArgGiven -statics])} {
	return -code error\
	    "conflicting values given for -statics and -noStatics"
    }
    if {$flag} {
	return [expr {!$noStatics}]
    } else {
	return $statics
    }
}

# Helper function to resolve the dual way of specifying nested loading
# (either by -nestedLoadOk or -nested 1)
proc ::safe::InterpNested {} {
    foreach v {Args nested nestedLoadOk} {
	upvar $v $v
    }
    set flag [::tcl::OptProcArgGiven -nestedLoadOk]
    # note that the test here is the opposite of the "InterpStatics" one
    # (it is not -noNested... because of the wanted default value)
    if {$flag && (!$nestedLoadOk != !$nested)
	&& ([::tcl::OptProcArgGiven -nested])} {
	return -code error\
	    "conflicting values given for -nested and -nestedLoadOk"
    }
    if {$flag} {
	# another difference with "InterpStatics"
	return $nestedLoadOk
    } else {
	return $nested
    }
}

####
#
#  API entry points that needs argument parsing :
#
####

# Interface/entry point function and front end for "Create"
proc ::safe::interpCreate {args} {
    variable AutoPathSync
    if {$AutoPathSync} {
        set autoPath {}
    }
    set Args [::tcl::OptKeyParse ::safe::interpCreate $args]
    RejectExcessColons $slave

    set withAutoPath [::tcl::OptProcArgGiven -autoPath]
    InterpCreate $slave $accessPath \
	[InterpStatics] [InterpNested] $deleteHook $autoPath $withAutoPath
}

proc ::safe::interpInit {args} {
    variable AutoPathSync
    if {$AutoPathSync} {
        set autoPath {}
    }
    set Args [::tcl::OptKeyParse ::safe::interpIC $args]
    if {![::interp exists $slave]} {
	return -code error "\"$slave\" is not an interpreter"
    }
    RejectExcessColons $slave

    set withAutoPath [::tcl::OptProcArgGiven -autoPath]
    InterpInit $slave $accessPath \
	[InterpStatics] [InterpNested] $deleteHook $autoPath $withAutoPath
}

# Check that the given slave is "one of us"
proc ::safe::CheckInterp {slave} {
    namespace upvar ::safe [VarName $slave] state
    if {![info exists state] || ![::interp exists $slave]} {
	return -code error \
	    "\"$slave\" is not an interpreter managed by ::safe::"
    }
}

# Interface/entry point function and front end for "Configure".  This code
# is awfully pedestrian because it would need more coupling and support
# between the way we store the configuration values in safe::interp's and
# the Opt package. Obviously we would like an OptConfigure to avoid
# duplicating all this code everywhere.
# -> TODO (the app should share or access easily the program/value stored
# by opt)

# This is even more complicated by the boolean flags with no values that
# we had the bad idea to support for the sake of user simplicity in
# create/init but which makes life hard in configure...
# So this will be hopefully written and some integrated with opt1.0
# (hopefully for tcl8.1 ?)
proc ::safe::interpConfigure {args} {
    variable AutoPathSync
    switch [llength $args] {
	1 {
	    # If we have exactly 1 argument the semantic is to return all
	    # the current configuration. We still call OptKeyParse though
	    # we know that "slave" is our given argument because it also
	    # checks for the "-help" option.
	    set Args [::tcl::OptKeyParse ::safe::interpIC $args]
	    CheckInterp $slave
	    namespace upvar ::safe [VarName $slave] state

	    set TMP [list \
		[list -accessPath $state(access_path)] \
		[list -statics    $state(staticsok)]   \
		[list -nested     $state(nestedok)]    \
	        [list -deleteHook $state(cleanupHook)] \
	    ]
	    if {!$AutoPathSync} {
	        lappend TMP [list -autoPath $state(auto_path)]
	    }
	    return [join $TMP]
	}
	2 {
	    # If we have exactly 2 arguments the semantic is a "configure
	    # get"
	    lassign $args slave arg

	    # get the flag sub program (we 'know' about Opt's internal
	    # representation of data)
	    set desc [lindex [::tcl::OptKeyGetDesc ::safe::interpIC] 2]
	    set hits [::tcl::OptHits desc $arg]
	    if {$hits > 1} {
		return -code error [::tcl::OptAmbigous $desc $arg]
	    } elseif {$hits == 0} {
		return -code error [::tcl::OptFlagUsage $desc $arg]
	    }
	    CheckInterp $slave
	    namespace upvar ::safe [VarName $slave] state

	    set item [::tcl::OptCurDesc $desc]
	    set name [::tcl::OptName $item]
	    switch -exact -- $name {
		-accessPath {
		    return [list -accessPath $state(access_path)]
		}
		-autoPath {
		    if {$AutoPathSync} {
		        return -code error "unknown flag $name (bug)"
		    } else {
		        return [list -autoPath $state(auto_path)]
		    }
		}
		-statics    {
		    return [list -statics $state(staticsok)]
		}
		-nested     {
		    return [list -nested $state(nestedok)]
		}
		-deleteHook {
		    return [list -deleteHook $state(cleanupHook)]
		}
		-noStatics {
		    # it is most probably a set in fact but we would need
		    # then to jump to the set part and it is not *sure*
		    # that it is a set action that the user want, so force
		    # it to use the unambigous -statics ?value? instead:
		    return -code error\
			"ambigous query (get or set -noStatics ?)\
				use -statics instead"
		}
		-nestedLoadOk {
		    return -code error\
			"ambigous query (get or set -nestedLoadOk ?)\
				use -nested instead"
		}
		default {
		    return -code error "unknown flag $name (bug)"
		}
	    }
	}
	default {
	    # Otherwise we want to parse the arguments like init and
	    # create did
	    set Args [::tcl::OptKeyParse ::safe::interpIC $args]
	    CheckInterp $slave
	    namespace upvar ::safe [VarName $slave] state

	    # Get the current (and not the default) values of whatever has
	    # not been given:
	    if {![::tcl::OptProcArgGiven -accessPath]} {
		set doreset 0
		set accessPath $state(access_path)
	    } else {
		set doreset 1
	    }
	    if {(!$AutoPathSync) && (![::tcl::OptProcArgGiven -autoPath])} {
		set autoPath $state(auto_path)
	    } elseif {$AutoPathSync} {
		set autoPath {}
	    } else {
	    }
	    if {
		![::tcl::OptProcArgGiven -statics]
		&& ![::tcl::OptProcArgGiven -noStatics]
	    } then {
		set statics    $state(staticsok)
	    } else {
		set statics    [InterpStatics]
	    }
	    if {
		[::tcl::OptProcArgGiven -nested] ||
		[::tcl::OptProcArgGiven -nestedLoadOk]
	    } then {
		set nested     [InterpNested]
	    } else {
		set nested     $state(nestedok)
	    }
	    if {![::tcl::OptProcArgGiven -deleteHook]} {
		set deleteHook $state(cleanupHook)
	    }
	    # we can now reconfigure :
	    set withAutoPath [::tcl::OptProcArgGiven -autoPath]
	    InterpSetConfig $slave $accessPath $statics $nested $deleteHook $autoPath $withAutoPath
	    # auto_reset the slave (to completly synch the new access_path)
	    if {$doreset} {
		if {[catch {::interp eval $slave {auto_reset}} msg]} {
		    Log $slave "auto_reset failed: $msg"
		} else {
		    Log $slave "successful auto_reset" NOTICE
		}

		# Sync the paths used to search for Tcl modules.
		::interp eval $slave {tcl::tm::path remove {*}[tcl::tm::list]}
		if {[llength $state(tm_path_slave)] > 0} {
		    ::interp eval $slave [list \
			    ::tcl::tm::add {*}[lreverse $state(tm_path_slave)]]
		}

		# Remove stale "package ifneeded" data for non-loaded packages.
		# - Not for loaded packages, because "package forget" erases
		#   data from "package provide" as well as "package ifneeded".
		# - This is OK because the script cannot reload any version of
		#   the package unless it first does "package forget".
		foreach pkg [::interp eval $slave {package names}] {
		    if {[::interp eval $slave [list package provide $pkg]] eq ""} {
			::interp eval $slave [list package forget $pkg]
		    }
		}
	    }
	    return
	}
    }
}

####
#
#  Functions that actually implements the exported APIs
#
####

#
# safe::InterpCreate : doing the real job
#
# This procedure creates a safe slave and initializes it with the safe
# base aliases.
# NB: slave name must be simple alphanumeric string, no spaces, no (), no
# {},...  {because the state array is stored as part of the name}
#
# Returns the slave name.
#
# Optional Arguments :
# + slave name : if empty, generated name will be used
# + access_path: path list controlling where load/source can occur,
#                if empty: the master auto_path and its subdirectories will be
#                used.
# + staticsok  : flag, if 0 :no static package can be loaded (load {} Xxx)
#                      if 1 :static packages are ok.
# + nestedok   : flag, if 0 :no loading to sub-sub interps (load xx xx sub)
#                      if 1 : multiple levels are ok.

# use the full name and no indent so auto_mkIndex can find us
proc ::safe::InterpCreate {
			   slave
			   access_path
			   staticsok
			   nestedok
			   deletehook
			   autoPath
			   withAutoPath
		       } {
    # Create the slave.
    # If evaluated in ::safe, the interpreter command for foo is ::foo;
    # but for foo::bar is safe::foo::bar.  So evaluate in :: instead.
    if {$slave ne ""} {
	namespace eval :: [list ::interp create -safe $slave]
    } else {
	# empty argument: generate slave name
	set slave [::interp create -safe]
    }
    Log $slave "Created" NOTICE

    # Initialize it. (returns slave name)
    InterpInit $slave $access_path $staticsok $nestedok $deletehook $autoPath $withAutoPath
}

#
# InterpSetConfig (was setAccessPath) :
#    Sets up slave virtual access path and corresponding structure within
#    the master. Also sets the tcl_library in the slave to be the first
#    directory in the path.
#    NB: If you change the path after the slave has been initialized you
#    probably need to call "auto_reset" in the slave in order that it gets
#    the right auto_index() array values.
#
#    It is the caller's responsibility, if it supplies a non-empty value for
#    access_path, to make the first directory in the path suitable for use as
#    tcl_library, and (if ![setSyncMode]), to set the slave's ::auto_path.

proc ::safe::InterpSetConfig {slave access_path staticsok nestedok deletehook autoPath withAutoPath} {
    global auto_path
    variable AutoPathSync

    # determine and store the access path if empty
    if {$access_path eq ""} {
	set access_path $auto_path

	# Make sure that tcl_library is in auto_path and at the first
	# position (needed by setAccessPath)
	set where [lsearch -exact $access_path [info library]]
	if {$where == -1} {
	    # not found, add it.
	    set access_path [linsert $access_path 0 [info library]]
	    Log $slave "tcl_library was not in auto_path,\
			added it to slave's access_path" NOTICE
	} elseif {$where != 0} {
	    # not first, move it first
	    set access_path [linsert \
				 [lreplace $access_path $where $where] \
				 0 [info library]]
	    Log $slave "tcl_libray was not in first in auto_path,\
			moved it to front of slave's access_path" NOTICE
	}

	set raw_auto_path $access_path

	# Add 1st level sub dirs (will searched by auto loading from tcl
	# code in the slave using glob and thus fail, so we add them here
	# so by default it works the same).
	set access_path [AddSubDirs $access_path]
    } else {
        set raw_auto_path $autoPath
    }

    if {$withAutoPath} {
        set raw_auto_path $autoPath
    }

    Log $slave "Setting accessPath=($access_path) staticsok=$staticsok\
		nestedok=$nestedok deletehook=($deletehook)" NOTICE
    if {!$AutoPathSync} {
        Log $slave "Setting auto_path=($raw_auto_path)" NOTICE
    }

    namespace upvar ::safe [VarName $slave] state

    # clear old autopath if it existed
    # build new one
    # Extend the access list with the paths used to look for Tcl Modules.
    # We save the virtual form separately as well, as syncing it with the
    # slave has to be defered until the necessary commands are present for
    # setup.
    set norm_access_path  {}
    set slave_access_path {}
    set map_access_path   {}
    set remap_access_path {}
    set slave_tm_path     {}

    set i 0
    foreach dir $access_path {
	set token [PathToken $i]
	lappend slave_access_path  $token
	lappend map_access_path    $token $dir
	lappend remap_access_path  $dir $token
	lappend norm_access_path   [file normalize $dir]
	incr i
    }

    # Set the slave auto_path to a tokenized raw_auto_path.
    # Silently ignore any directories that are not in the access path.
    # If [setSyncMode], SyncAccessPath will overwrite this value with the
    # full access path.
    # If ![setSyncMode], Safe Base code will not change this value.
    set tokens_auto_path {}
    foreach dir $raw_auto_path {
	if {[dict exists $remap_access_path $dir]} {
	    lappend tokens_auto_path [dict get $remap_access_path $dir]
	}
    }
    ::interp eval $slave [list set auto_path $tokens_auto_path]

    # Add the tcl::tm directories to the access path.
    set morepaths [::tcl::tm::list]
    set firstpass 1
    while {[llength $morepaths]} {
	set addpaths $morepaths
	set morepaths {}

	foreach dir $addpaths {
	    # Prevent the addition of dirs on the tm list to the
	    # result if they are already known.
	    if {[dict exists $remap_access_path $dir]} {
	        if {$firstpass} {
		    # $dir is in [::tcl::tm::list] and belongs in the slave_tm_path.
		    # Later passes handle subdirectories, which belong in the
		    # access path but not in the module path.
		    lappend slave_tm_path  [dict get $remap_access_path $dir]
		}
		continue
	    }

	    set token [PathToken $i]
	    lappend access_path        $dir
	    lappend slave_access_path  $token
	    lappend map_access_path    $token $dir
	    lappend remap_access_path  $dir $token
	    lappend norm_access_path   [file normalize $dir]
	    if {$firstpass} {
		# $dir is in [::tcl::tm::list] and belongs in the slave_tm_path.
		# Later passes handle subdirectories, which belong in the
		# access path but not in the module path.
		lappend slave_tm_path  $token
	    }
	    incr i

	    # [Bug 2854929]
	    # Recursively find deeper paths which may contain
	    # modules. Required to handle modules with names like
	    # 'platform::shell', which translate into
	    # 'platform/shell-X.tm', i.e arbitrarily deep
	    # subdirectories.
	    lappend morepaths {*}[glob -nocomplain -directory $dir -type d *]
	}
	set firstpass 0
    }

    set state(access_path)       $access_path
    set state(access_path,map)   $map_access_path
    set state(access_path,remap) $remap_access_path
    set state(access_path,norm)  $norm_access_path
    set state(access_path,slave) $slave_access_path
    set state(tm_path_slave)     $slave_tm_path
    set state(staticsok)         $staticsok
    set state(nestedok)          $nestedok
    set state(cleanupHook)       $deletehook

    if {!$AutoPathSync} {
        set state(auto_path)     $raw_auto_path
    }

    SyncAccessPath $slave
    return
}


#
# DetokPath:
#    Convert tokens to directories where possible.
#    Leave undefined tokens unconverted.  They are
#    nonsense in both the slave and the master.
#
proc ::safe::DetokPath {slave tokenPath} {
    namespace upvar ::safe S$slave state

    set slavePath {}
    foreach token $tokenPath {
	if {[dict exists $state(access_path,map) $token]} {
	    lappend slavePath [dict get $state(access_path,map) $token]
	} else {
	    lappend slavePath $token
	}
    }
    return $slavePath
}

#
#
# interpFindInAccessPath:
#    Search for a real directory and returns its virtual Id (including the
#    "$")
#
#    When debugging, use TranslatePath for the inverse operation.
proc ::safe::interpFindInAccessPath {slave path} {
    CheckInterp $slave
    namespace upvar ::safe [VarName $slave] state

    if {![dict exists $state(access_path,remap) $path]} {
	return -code error "$path not found in access path"
    }

    return [dict get $state(access_path,remap) $path]
}


#
# addToAccessPath:
#    add (if needed) a real directory to access path and return its
#    virtual token (including the "$").
proc ::safe::interpAddToAccessPath {slave path} {
    # first check if the directory is already in there
    # (inlined interpFindInAccessPath).
    CheckInterp $slave
    namespace upvar ::safe [VarName $slave] state

    if {[dict exists $state(access_path,remap) $path]} {
	return [dict get $state(access_path,remap) $path]
    }

    # new one, add it:
    set token [PathToken [llength $state(access_path)]]

    lappend state(access_path)       $path
    lappend state(access_path,slave) $token
    lappend state(access_path,map)   $token $path
    lappend state(access_path,remap) $path $token
    lappend state(access_path,norm)  [file normalize $path]

    SyncAccessPath $slave
    return $token
}

# This procedure applies the initializations to an already existing
# interpreter. It is useful when you want to install the safe base aliases
# into a preexisting safe interpreter.
proc ::safe::InterpInit {
			 slave
			 access_path
			 staticsok
			 nestedok
			 deletehook
			 autoPath
			 withAutoPath
		     } {
    # Configure will generate an access_path when access_path is empty.
    InterpSetConfig $slave $access_path $staticsok $nestedok $deletehook $autoPath $withAutoPath

    # NB we need to add [namespace current], aliases are always absolute
    # paths.

    # These aliases let the slave load files to define new commands
    # This alias lets the slave use the encoding names, convertfrom,
    # convertto, and system, but not "encoding system <name>" to set the
    # system encoding.
    # Handling Tcl Modules, we need a restricted form of Glob.
    # This alias interposes on the 'exit' command and cleanly terminates
    # the slave.

    foreach {command alias} {
	source   AliasSource
	load     AliasLoad
	encoding AliasEncoding
	exit     interpDelete
	glob     AliasGlob
    } {
	::interp alias $slave $command {} [namespace current]::$alias $slave
    }

    # This alias lets the slave have access to a subset of the 'file'
    # command functionality.

    ::interp expose $slave file
    foreach subcommand {dirname extension rootname tail} {
	::interp alias $slave ::tcl::file::$subcommand {} \
	    ::safe::AliasFileSubcommand $slave $subcommand
    }
    foreach subcommand {
	atime attributes copy delete executable exists isdirectory isfile
	link lstat mtime mkdir nativename normalize owned readable readlink
	rename size stat tempfile type volumes writable
    } {
	::interp alias $slave ::tcl::file::$subcommand {} \
	    ::safe::BadSubcommand $slave file $subcommand
    }

    # Subcommands of info
    foreach {subcommand alias} {
	nameofexecutable   AliasExeName
    } {
	::interp alias $slave ::tcl::info::$subcommand \
	    {} [namespace current]::$alias $slave
    }

    # The allowed slave variables already have been set by Tcl_MakeSafe(3)

    # Source init.tcl and tm.tcl into the slave, to get auto_load and
    # other procedures defined:

    if {[catch {::interp eval $slave {
	source [file join $tcl_library init.tcl]
    }} msg opt]} {
	Log $slave "can't source init.tcl ($msg)"
	return -options $opt "can't source init.tcl into slave $slave ($msg)"
    }

    if {[catch {::interp eval $slave {
	source [file join $tcl_library tm.tcl]
    }} msg opt]} {
	Log $slave "can't source tm.tcl ($msg)"
	return -options $opt "can't source tm.tcl into slave $slave ($msg)"
    }

    # Sync the paths used to search for Tcl modules. This can be done only
    # now, after tm.tcl was loaded.
    namespace upvar ::safe [VarName $slave] state
    if {[llength $state(tm_path_slave)] > 0} {
	::interp eval $slave [list \
		::tcl::tm::add {*}[lreverse $state(tm_path_slave)]]
    }
    return $slave
}

# Add (only if needed, avoid duplicates) 1 level of sub directories to an
# existing path list.  Also removes non directories from the returned
# list.
proc ::safe::AddSubDirs {pathList} {
    set res {}
    foreach dir $pathList {
	if {[file isdirectory $dir]} {
	    # check that we don't have it yet as a children of a previous
	    # dir
	    if {$dir ni $res} {
		lappend res $dir
	    }
	    foreach sub [glob -directory $dir -nocomplain *] {
		if {[file isdirectory $sub] && ($sub ni $res)} {
		    # new sub dir, add it !
		    lappend res $sub
		}
	    }
	}
    }
    return $res
}

# This procedure deletes a safe slave managed by Safe Tcl and cleans up
# associated state.
# - The command will also delete non-Safe-Base interpreters.
# - This is regrettable, but to avoid breaking existing code this should be
#   amended at the next major revision by uncommenting "CheckInterp".

proc ::safe::interpDelete {slave} {
    Log $slave "About to delete" NOTICE

    # CheckInterp $slave
    namespace upvar ::safe [VarName $slave] state

    # When an interpreter is deleted with [interp delete], any sub-interpreters
    # are deleted automatically, but this leaves behind their data in the Safe
    # Base. To clean up properly, we call safe::interpDelete recursively on each
    # Safe Base sub-interpreter, so each one is deleted cleanly and not by
    # the automatic mechanism built into [interp delete].
    foreach sub [interp slaves $slave] {
        if {[info exists ::safe::[VarName [list $slave $sub]]]} {
            ::safe::interpDelete [list $slave $sub]
        }
    }

    # If the slave has a cleanup hook registered, call it.  Check the
    # existance because we might be called to delete an interp which has
    # not been registered with us at all

    if {[info exists state(cleanupHook)]} {
	set hook $state(cleanupHook)
	if {[llength $hook]} {
	    # remove the hook now, otherwise if the hook calls us somehow,
	    # we'll loop
	    unset state(cleanupHook)
	    try {
		{*}$hook $slave
	    } on error err {
		Log $slave "Delete hook error ($err)"
	    }
	}
    }

    # Discard the global array of state associated with the slave, and
    # delete the interpreter.

    if {[info exists state]} {
	unset state
    }

    # if we have been called twice, the interp might have been deleted
    # already
    if {[::interp exists $slave]} {
	::interp delete $slave
	Log $slave "Deleted" NOTICE
    }

    return
}

# Set (or get) the logging mecanism

proc ::safe::setLogCmd {args} {
    variable Log
    set la [llength $args]
    if {$la == 0} {
	return $Log
    } elseif {$la == 1} {
	set Log [lindex $args 0]
    } else {
	set Log $args
    }

    if {$Log eq ""} {
	# Disable logging completely. Calls to it will be compiled out
	# of all users.
	proc ::safe::Log {args} {}
    } else {
	# Activate logging, define proper command.

	proc ::safe::Log {slave msg {type ERROR}} {
	    variable Log
	    {*}$Log "$type for slave $slave : $msg"
	    return
	}
    }
}

# ------------------- END OF PUBLIC METHODS ------------

#
# Sets the slave auto_path to its recorded access path.  Also sets
# tcl_library to the first token of the access path.
#
proc ::safe::SyncAccessPath {slave} {
    variable AutoPathSync
    namespace upvar ::safe [VarName $slave] state

    set slave_access_path $state(access_path,slave)
    if {$AutoPathSync} {
	::interp eval $slave [list set auto_path $slave_access_path]

	Log $slave "auto_path in $slave has been set to $slave_access_path"\
		NOTICE
    }

    # This code assumes that info library is the first element in the
    # list of access path's. See -> InterpSetConfig for the code which
    # ensures this condition.

    ::interp eval $slave [list \
	      set tcl_library [lindex $slave_access_path 0]]
    return
}

# Returns the virtual token for directory number N.
proc ::safe::PathToken {n} {
    # We need to have a ":" in the token string so [file join] on the
    # mac won't turn it into a relative path.
    return "\$p(:$n:)" ;# Form tested by case 7.2
}

#
# translate virtual path into real path
#
proc ::safe::TranslatePath {slave path} {
    namespace upvar ::safe [VarName $slave] state

    # somehow strip the namespaces 'functionality' out (the danger is that
    # we would strip valid macintosh "../" queries... :
    if {[string match "*::*" $path] || [string match "*..*" $path]} {
	return -code error "invalid characters in path $path"
    }

    # Use a cached map instead of computed local vars and subst.

    return [string map $state(access_path,map) $path]
}

# file name control (limit access to files/resources that should be a
# valid tcl source file)
proc ::safe::CheckFileName {slave file} {
    # This used to limit what can be sourced to ".tcl" and forbid files
    # with more than 1 dot and longer than 14 chars, but I changed that
    # for 8.4 as a safe interp has enough internal protection already to
    # allow sourcing anything. - hobbs

    if {![file exists $file]} {
	# don't tell the file path
	return -code error "no such file or directory"
    }

    if {![file readable $file]} {
	# don't tell the file path
	return -code error "not readable"
    }
}

# AliasFileSubcommand handles selected subcommands of [file] in safe
# interpreters that are *almost* safe. In particular, it just acts to
# prevent discovery of what home directories exist.

proc ::safe::AliasFileSubcommand {slave subcommand name} {
    if {[string match ~* $name]} {
	set name ./$name
    }
    tailcall ::interp invokehidden $slave tcl:file:$subcommand $name
}

# AliasGlob is the target of the "glob" alias in safe interpreters.

proc ::safe::AliasGlob {slave args} {
    variable AutoPathSync
    Log $slave "GLOB ! $args" NOTICE
    set cmd {}
    set at 0
    array set got {
	-directory 0
	-nocomplain 0
	-join 0
	-tails 0
	-- 0
    }

    if {$::tcl_platform(platform) eq "windows"} {
	set dirPartRE {^(.*)[\\/]([^\\/]*)$}
    } else {
	set dirPartRE {^(.*)/([^/]*)$}
    }

    set dir        {}
    set virtualdir {}

    while {$at < [llength $args]} {
	switch -glob -- [set opt [lindex $args $at]] {
	    -nocomplain - -- - -tails {
		lappend cmd $opt
		set got($opt) 1
		incr at
	    }
	    -join {
		set got($opt) 1
		incr at
	    }
	    -types - -type {
		lappend cmd -types [lindex $args [incr at]]
		incr at
	    }
	    -directory {
		if {$got($opt)} {
		    return -code error \
			{"-directory" cannot be used with "-path"}
		}
		set got($opt) 1
		set virtualdir [lindex $args [incr at]]
		incr at
	    }
	    -* {
		Log $slave "Safe base rejecting glob option '$opt'"
		return -code error "Safe base rejecting glob option '$opt'"
		# unsafe/unnecessary options rejected: -path
	    }
	    default {
		break
	    }
	}
	if {$got(--)} break
    }

    # Get the real path from the virtual one and check that the path is in the
    # access path of that slave. Done after basic argument processing so that
    # we know if -nocomplain is set.
    if {$got(-directory)} {
	try {
	    set dir [TranslatePath $slave $virtualdir]
	    DirInAccessPath $slave $dir
	} on error msg {
	    Log $slave $msg
	    if {$got(-nocomplain)} return
	    return -code error "permission denied"
	}
	if {$got(--)} {
	    set cmd [linsert $cmd end-1 -directory $dir]
	} else {
	    lappend cmd -directory $dir
	}
    } else {
	# The code after this "if ... else" block would conspire to return with
	# no results in this case, if it were allowed to proceed.  Instead,
	# return now and reduce the number of cases to be considered later.
	Log $slave {option -directory must be supplied}
	if {$got(-nocomplain)} return
	return -code error "permission denied"
    }

    # Apply the -join semantics ourselves (hence -join not copied to $cmd)
    if {$got(-join)} {
	set args [lreplace $args $at end [join [lrange $args $at end] "/"]]
    }

    # Process the pattern arguments.  If we've done a join there is only one
    # pattern argument.

    set firstPattern [llength $cmd]
    foreach opt [lrange $args $at end] {
	if {![regexp $dirPartRE $opt -> thedir thefile]} {
	    set thedir .
	    # The *.tm search comes here.
	}
	# "Special" treatment for (joined) argument {*/pkgIndex.tcl}.
	# Do the expansion of "*" here, and filter out any directories that are
	# not in the access path.  The outcome is to lappend to cmd a path of
	# the form $virtualdir/subdir/pkgIndex.tcl for each subdirectory subdir,
	# after removing any subdir that are not in the access path.
	if {($thedir eq "*") && ($thefile eq "pkgIndex.tcl")} {
	    set mapped 0
	    foreach d [glob -directory [TranslatePath $slave $virtualdir] \
			   -types d -tails *] {
		catch {
		    DirInAccessPath $slave \
			[TranslatePath $slave [file join $virtualdir $d]]
		    lappend cmd [file join $d $thefile]
		    set mapped 1
		}
	    }
	    if {$mapped} continue
	    # Don't [continue] if */pkgIndex.tcl has no matches in the access
	    # path.  The pattern will now receive the same treatment as a
	    # "non-special" pattern (and will fail because it includes a "*" in
	    # the directory name).
	}
	# Any directory pattern that is not an exact (i.e. non-glob) match to a
	# directory in the access path will be rejected here.
	# - Rejections include any directory pattern that has glob matching
	#   patterns "*", "?", backslashes, braces or square brackets, (UNLESS
	#   it corresponds to a genuine directory name AND that directory is in
	#   the access path).
	# - The only "special matching characters" that remain in patterns for
	#   processing by glob are in the filename tail.
	# - [file join $anything ~${foo}] is ~${foo}, which is not an exact
	#   match to any directory in the access path.  Hence directory patterns
	#   that begin with "~" are rejected here.  Tests safe-16.[5-8] check
	#   that "file join" remains as required and does not expand ~${foo}.
	# - Bug [3529949] relates to unwanted expansion of ~${foo} and this is
	#   how the present code avoids the bug.  All tests safe-16.* relate.
	try {
	    DirInAccessPath $slave [TranslatePath $slave \
		    [file join $virtualdir $thedir]]
	} on error msg {
	    Log $slave $msg
	    if {$got(-nocomplain)} continue
	    return -code error "permission denied"
	}
	lappend cmd $opt
    }

    Log $slave "GLOB = $cmd" NOTICE

    if {$got(-nocomplain) && [llength $cmd] eq $firstPattern} {
	return
    }
    try {
	# >>>>>>>>>> HERE'S THE CALL TO SAFE INTERP GLOB <<<<<<<<<<
	# - Pattern arguments added to cmd have NOT been translated from tokens.
	#   Only the virtualdir is translated (to dir).
	# - In the pkgIndex.tcl case, there is no "*" in the pattern arguments,
	#   which are a list of names each with tail pkgIndex.tcl.  The purpose
	#   of the call to glob is to remove the names for which the file does
	#   not exist.
	set entries [::interp invokehidden $slave glob {*}$cmd]
    } on error msg {
	# This is the only place that a call with -nocomplain and no invalid
	# "dash-options" can return an error.
	Log $slave $msg
	return -code error "script error"
    }

    Log $slave "GLOB < $entries" NOTICE

    # Translate path back to what the slave should see.
    set res {}
    set l [string length $dir]
    foreach p $entries {
	if {[string equal -length $l $dir $p]} {
	    set p [string replace $p 0 [expr {$l-1}] $virtualdir]
	}
	lappend res $p
    }

    Log $slave "GLOB > $res" NOTICE
    return $res
}

# AliasSource is the target of the "source" alias in safe interpreters.

proc ::safe::AliasSource {slave args} {
    set argc [llength $args]
    # Extended for handling of Tcl Modules to allow not only "source
    # filename", but "source -encoding E filename" as well.
    if {[lindex $args 0] eq "-encoding"} {
	incr argc -2
	set encoding [lindex $args 1]
	set at 2
	if {$encoding eq "identity"} {
	    Log $slave "attempt to use the identity encoding"
	    return -code error "permission denied"
	}
    } else {
	set at 0
	set encoding {}
    }
    if {$argc != 1} {
	set msg "wrong # args: should be \"source ?-encoding E? fileName\""
	Log $slave "$msg ($args)"
	return -code error $msg
    }
    set file [lindex $args $at]

    # get the real path from the virtual one.
    if {[catch {
	set realfile [TranslatePath $slave $file]
    } msg]} {
	Log $slave $msg
	return -code error "permission denied"
    }

    # check that the path is in the access path of that slave
    if {[catch {
	FileInAccessPath $slave $realfile
    } msg]} {
	Log $slave $msg
	return -code error "permission denied"
    }

    # Check that the filename exists and is readable.  If it is not, deliver
    # this -errorcode so that caller in tclPkgUnknown does not write a message
    # to tclLog.  Has no effect on other callers of ::source, which are in
    # "package ifneeded" scripts.
    if {[catch {
	CheckFileName $slave $realfile
    } msg]} {
	Log $slave "$realfile:$msg"
	return -code error -errorcode {POSIX EACCES} $msg
    }

    # Passed all the tests, lets source it. Note that we do this all manually
    # because we want to control [info script] in the slave so information
    # doesn't leak so much. [Bug 2913625]
    set old [::interp eval $slave {info script}]
    set replacementMsg "script error"
    set code [catch {
	set f [open $realfile]
	fconfigure $f -eofchar \032
	if {$encoding ne ""} {
	    fconfigure $f -encoding $encoding
	}
	set contents [read $f]
	close $f
	::interp eval $slave [list info script $file]
    } msg opt]
    if {$code == 0} {
	set code [catch {::interp eval $slave $contents} msg opt]
	set replacementMsg $msg
    }
    catch {interp eval $slave [list info script $old]}
    # Note that all non-errors are fine result codes from [source], so we must
    # take a little care to do it properly. [Bug 2923613]
    if {$code == 1} {
	Log $slave $msg
	return -code error $replacementMsg
    }
    return -code $code -options $opt $msg
}

# AliasLoad is the target of the "load" alias in safe interpreters.

proc ::safe::AliasLoad {slave file args} {
    set argc [llength $args]
    if {$argc > 2} {
	set msg "load error: too many arguments"
	Log $slave "$msg ($argc) {$file $args}"
	return -code error $msg
    }

    # package name (can be empty if file is not).
    set package [lindex $args 0]

    namespace upvar ::safe [VarName $slave] state

    # Determine where to load. load use a relative interp path and {}
    # means self, so we can directly and safely use passed arg.
    set target [lindex $args 1]
    if {$target ne ""} {
	# we will try to load into a sub sub interp; check that we want to
	# authorize that.
	if {!$state(nestedok)} {
	    Log $slave "loading to a sub interp (nestedok)\
			disabled (trying to load $package to $target)"
	    return -code error "permission denied (nested load)"
	}
    }

    # Determine what kind of load is requested
    if {$file eq ""} {
	# static package loading
	if {$package eq ""} {
	    set msg "load error: empty filename and no package name"
	    Log $slave $msg
	    return -code error $msg
	}
	if {!$state(staticsok)} {
	    Log $slave "static packages loading disabled\
			(trying to load $package to $target)"
	    return -code error "permission denied (static package)"
	}
    } else {
	# file loading

	# get the real path from the virtual one.
	try {
	    set file [TranslatePath $slave $file]
	} on error msg {
	    Log $slave $msg
	    return -code error "permission denied"
	}

	# check the translated path
	try {
	    FileInAccessPath $slave $file
	} on error msg {
	    Log $slave $msg
	    return -code error "permission denied (path)"
	}
    }

    try {
	return [::interp invokehidden $slave load $file $package $target]
    } on error msg {
	Log $slave $msg
	return -code error $msg
    }
}

# FileInAccessPath raises an error if the file is not found in the list of
# directories contained in the (master side recorded) slave's access path.

# the security here relies on "file dirname" answering the proper
# result... needs checking ?
proc ::safe::FileInAccessPath {slave file} {
    namespace upvar ::safe [VarName $slave] state
    set access_path $state(access_path)

    if {[file isdirectory $file]} {
	return -code error "\"$file\": is a directory"
    }
    set parent [file dirname $file]

    # Normalize paths for comparison since lsearch knows nothing of
    # potential pathname anomalies.
    set norm_parent [file normalize $parent]

    namespace upvar ::safe [VarName $slave] state
    if {$norm_parent ni $state(access_path,norm)} {
	return -code error "\"$file\": not in access_path"
    }
}

proc ::safe::DirInAccessPath {slave dir} {
    namespace upvar ::safe [VarName $slave] state
    set access_path $state(access_path)

    if {[file isfile $dir]} {
	return -code error "\"$dir\": is a file"
    }

    # Normalize paths for comparison since lsearch knows nothing of
    # potential pathname anomalies.
    set norm_dir [file normalize $dir]

    namespace upvar ::safe [VarName $slave] state
    if {$norm_dir ni $state(access_path,norm)} {
	return -code error "\"$dir\": not in access_path"
    }
}

# This procedure is used to report an attempt to use an unsafe member of an
# ensemble command.

proc ::safe::BadSubcommand {slave command subcommand args} {
    set msg "not allowed to invoke subcommand $subcommand of $command"
    Log $slave $msg
    return -code error -errorcode {TCL SAFE SUBCOMMAND} $msg
}

# AliasEncoding is the target of the "encoding" alias in safe interpreters.

proc ::safe::AliasEncoding {slave option args} {
    # Note that [encoding dirs] is not supported in safe slaves at all
    set subcommands {convertfrom convertto names system}
    try {
	set option [tcl::prefix match -error [list -level 1 -errorcode \
		[list TCL LOOKUP INDEX option $option]] $subcommands $option]
	# Special case: [encoding system] ok, but [encoding system foo] not
	if {$option eq "system" && [llength $args]} {
	    return -code error -errorcode {TCL WRONGARGS} \
		"wrong # args: should be \"encoding system\""
	}
    } on error {msg options} {
	Log $slave $msg
	return -options $options $msg
    }
    tailcall ::interp invokehidden $slave encoding $option {*}$args
}

# Various minor hiding of platform features. [Bug 2913625]

proc ::safe::AliasExeName {slave} {
    return ""
}

# ------------------------------------------------------------------------------
# Using Interpreter Names with Namespace Qualifiers
# ------------------------------------------------------------------------------
# (1) We wish to preserve compatibility with existing code, in which Safe Base
#     interpreter names have no namespace qualifiers.
# (2) safe::interpCreate and the rest of the Safe Base previously could not
#     accept namespace qualifiers in an interpreter name.
# (3) The interp command will accept namespace qualifiers in an interpreter
#     name, but accepts distinct interpreters that will have the same command
#     name (e.g. foo, ::foo, and :::foo) (bug 66c2e8c974).
# (4) To satisfy these constraints, Safe Base interpreter names will be fully
#     qualified namespace names with no excess colons and with the leading "::"
#     omitted.
# (5) Trailing "::" implies a namespace tail {}, which interp reads as {{}}.
#     Reject such names.
# (6) We could:
#     (a) EITHER reject usable but non-compliant names (e.g. excess colons) in
#         interpCreate, interpInit;
#     (b) OR accept such names and then translate to a compliant name in every
#         command.
#     The problem with (b) is that the user will expect to use the name with the
#     interp command and will find that it is not recognised.
#     E.g "interpCreate ::foo" creates interpreter "foo", and the user's name
#     "::foo" works with all the Safe Base commands, but "interp eval ::foo"
#     fails.
#     So we choose (a).
# (7) The command
#         namespace upvar ::safe S$slave state
#     becomes
#         namespace upvar ::safe [VarName $slave] state
# ------------------------------------------------------------------------------

proc ::safe::RejectExcessColons {slave} {
    set stripped [regsub -all -- {:::*} $slave ::]
    if {[string range $stripped end-1 end] eq {::}} {
        return -code error {interpreter name must not end in "::"}
    }
    if {$stripped ne $slave} {
        set msg {interpreter name has excess colons in namespace separators}
        return -code error $msg
    }
    if {[string range $stripped 0 1] eq {::}} {
        return -code error {interpreter name must not begin "::"}
    }
    return
}

proc ::safe::VarName {slave} {
    # return S$slave
    return S[string map {:: @N @ @A} $slave]
}

proc ::safe::Setup {} {
    ####
    #
    # Setup the arguments parsing
    #
    ####
    variable AutoPathSync

    # Share the descriptions
    set OptList {
	{-accessPath -list {} "access path for the slave"}
	{-noStatics "prevent loading of statically linked pkgs"}
	{-statics true "loading of statically linked pkgs"}
	{-nestedLoadOk "allow nested loading"}
	{-nested false "nested loading"}
	{-deleteHook -script {} "delete hook"}
    }
    if {!$AutoPathSync} {
        lappend OptList {-autoPath -list {} "::auto_path for the slave"}
    }
    set temp [::tcl::OptKeyRegister $OptList]

    # create case (slave is optional)
    ::tcl::OptKeyRegister {
	{?slave? -name {} "name of the slave (optional)"}
    } ::safe::interpCreate

    # adding the flags sub programs to the command program (relying on Opt's
    # internal implementation details)
    lappend ::tcl::OptDesc(::safe::interpCreate) $::tcl::OptDesc($temp)

    # init and configure (slave is needed)
    ::tcl::OptKeyRegister {
	{slave -name {} "name of the slave"}
    } ::safe::interpIC

    # adding the flags sub programs to the command program (relying on Opt's
    # internal implementation details)
    lappend ::tcl::OptDesc(::safe::interpIC) $::tcl::OptDesc($temp)

    # temp not needed anymore
    ::tcl::OptKeyDelete $temp

    ####
    #
    # Default: No logging.
    #
    ####

    setLogCmd {}

    # Log eventually.
    # To enable error logging, set Log to {puts stderr} for instance,
    # via setLogCmd.
    return
}

# Accessor method for ::safe::AutoPathSync
# Usage: ::safe::setSyncMode ?newValue?
# Respond to changes by calling Setup again, preserving any
# caller-defined logging.  This allows complete equivalence with
# prior Safe Base behavior if AutoPathSync is true.
#
#                   >>> WARNING <<<
#
# DO NOT CHANGE AutoPathSync EXCEPT BY THIS COMMAND - IT IS VITAL THAT WHENEVER
# THE VALUE CHANGES, THE EXISTING PARSE TOKENS ARE DELETED AND Setup IS CALLED
# AGAIN.
# (The initialization of AutoPathSync at the end of this file is acceptable
#  because Setup has not yet been called.)

proc ::safe::setSyncMode {args} {
    variable AutoPathSync

    if {[llength $args] == 0} {
    } elseif {[llength $args] == 1} {
        set newValue [lindex $args 0]
        if {![string is boolean -strict $newValue]} {
            return -code error "new value must be a valid boolean"
        }
        set args [expr {$newValue && $newValue}]
        if {([info vars ::safe::S*] ne {}) && ($args != $AutoPathSync)} {
            return -code error \
                    "cannot set new value while Safe Base slaves exist"
        }
        if {($args != $AutoPathSync)} {
            set AutoPathSync {*}$args
            ::tcl::OptKeyDelete ::safe::interpCreate
            ::tcl::OptKeyDelete ::safe::interpIC
            set TmpLog [setLogCmd]
            Setup
            setLogCmd $TmpLog
        }
    } else {
        set msg {wrong # args: should be "safe::setSyncMode ?newValue?"}
        return -code error $msg
    }

    return $AutoPathSync
}

namespace eval ::safe {
    # internal variables (must not begin with "S")

    # AutoPathSync
    #
    # Set AutoPathSync to 0 to give a slave's ::auto_path the same meaning as
    # for an unsafe interpreter: the package command will search its directories
    # and first-level subdirectories for pkgIndex.tcl files; the auto-loader
    # will search its directories for tclIndex files.  The access path and
    # module path will be maintained as separate values, and ::auto_path will
    # not be updated when the user calls ::safe::interpAddToAccessPath to add to
    # the access path.  If the user specifies an access path when calling
    # interpCreate, interpInit or interpConfigure, it is the user's
    # responsibility to define the slave's auto_path.  If these commands are
    # called with no (or empty) access path, the slave's auto_path will be set
    # to a tokenized form of the master's auto_path, and these directories and
    # their first-level subdirectories will be added to the access path.
    #
    # Set to 1 for "traditional" behavior: a slave's entire access path and
    # module path are copied to its ::auto_path, which is updated whenever
    # the user calls ::safe::interpAddToAccessPath to add to the access path.
    variable AutoPathSync 1

    # Log command, set via 'setLogCmd'. Logging is disabled when empty.
    variable Log {}

    # The package maintains a state array per slave interp under its
    # control. The name of this array is S<interp-name>. This array is
    # brought into scope where needed, using 'namespace upvar'. The S
    # prefix is used to avoid that a slave interp called "Log" smashes
    # the "Log" variable.
    #
    # The array's elements are:
    #
    # access_path       : List of paths accessible to the slave.
    # access_path,norm  : Ditto, in normalized form.
    # access_path,slave : Ditto, as the path tokens as seen by the slave.
    # access_path,map   : dict ( token -> path )
    # access_path,remap : dict ( path -> token )
    # auto_path         : List of paths requested by the caller as slave's ::auto_path.
    # tm_path_slave     : List of TM root directories, as tokens seen by the slave.
    # staticsok         : Value of option -statics
    # nestedok          : Value of option -nested
    # cleanupHook       : Value of option -deleteHook
    #
    # In principle, the slave can change its value of ::auto_path -
    # - a package might add a path (that is already in the access path) for
    #   access to tclIndex files;
    # - the script might remove some elements of the auto_path.
    # However, this is really the business of the master, and the auto_path will
    # be reset whenever the token mapping changes (i.e. when option -accessPath is
    # used to change the access path).
    # -autoPath is now stored in the array and is no longer obtained from
    # the slave.
}

::safe::Setup
