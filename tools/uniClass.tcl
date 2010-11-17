#!/bin/sh
# The next line is executed by /bin/sh, but not tcl \
exec tclsh "$0" ${1+"$@"}

#
# uniClass.tcl --
#
#	Generates the character ranges and singletons that are used in
#	generic/regc_locale.c for translation of character classes.
#	This file must be generated using a tclsh that contains the
#	correct corresponding tclUniData.c file (generated by uniParse.tcl)
#	in order for the class ranges to match.
#

proc emitRange {first last} {
    global ranges numranges chars numchars

    if {$first < ($last-1)} {
	append ranges [format "{0x%04x, 0x%04x}, " \
		$first $last]
	if {[incr numranges] % 4 == 0} {
	    append ranges "\n    "
	}
    } else {
	append chars [format "0x%04x, " $first]
	incr numchars
	if {$numchars % 9 == 0} {
	    append chars "\n    "
	}
	if {$first != $last} {
	    append chars [format "0x%04x, " $last]
	    incr numchars
	    if {$numchars % 9 == 0} {
		append chars "\n    "
	    }
	}
    }
}

proc genTable {type} {
    global first last ranges numranges chars numchars
    set first -2
    set last -2

    set ranges "    "
    set numranges 0
    set chars "    "
    set numchars 0

    for {set i 0} {$i <= 0xFFFF} {incr i} {
	if {[string is $type [format %c $i]]} {
	    if {$i == ($last + 1)} {
		set last $i
	    } else {
		if {$first > 0} {
		    emitRange $first $last
		}
		set first $i
		set last $i
	    }
	}
    }
    emitRange $first $last

    set ranges [string trimright $ranges "\t\n ,"]
    set chars  [string trimright $chars "\t\n ,"]
    if {$ranges ne ""} {
	puts "static const crange ${type}RangeTable\[\] = {\n$ranges\n};\n"
	puts "#define NUM_[string toupper $type]_RANGE (sizeof(${type}RangeTable)/sizeof(crange))\n"
    } else {
	puts "/* no contiguous ranges of $type characters */\n"
    }
    if {$chars ne ""} {
	puts "static const chr ${type}CharTable\[\] = {\n$chars\n};\n"
	puts "#define NUM_[string toupper $type]_CHAR (sizeof(${type}CharTable)/sizeof(chr))\n"
    } else {
	puts "/*\n * no singletons of $type characters.\n */\n"
    }
}

puts "/*
 *	Declarations of Unicode character ranges.  This code
 *	is automatically generated by the tools/uniClass.tcl script
 *	and used in generic/regc_locale.c.  Do not modify by hand.
 */
"

foreach {type desc} {
    alpha "alphabetic characters"
    digit "decimal digit characters"
    punct "punctuation characters"
    space "white space characters"
    lower "lowercase characters"
    upper "uppercase characters"
    graph "unicode print characters excluding space"
} {
    puts "/*\n * Unicode: $desc.\n */\n"
    genTable $type
}

puts "/*
 *	End of auto-generated Unicode character ranges declarations.
 */"
