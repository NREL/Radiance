# SCCSid "$SunId$ LBL"
#
# General utility routines
#

proc beep {} {		# ring the bell
	puts -nonewline stderr "\a"
}

proc view_txt fn {		# view a text file
	global env
	if [info exists env(EDITOR)] {
		eval exec xterm -e $env(EDITOR) $fn
	} else {
		foreach f $fn {
			exec xedit $f
		}
	}
}

proc cardval {var cvl} {	# set variable to one of a set of values
	upvar $var v
	set tv [string toupper $v]
	foreach cv $cvl {
		if {[string first $tv [string toupper $cv]] == 0} {
			set v $cv
			return $cv
		}
	}
	return {} ;		# this would seem to be an error
}
