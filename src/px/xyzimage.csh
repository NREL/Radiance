#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Display one or more CIE XYZE pictures using ximage
#
set popt=""
if ( $?DISPLAY_PRIMARIES ) then
	set popt="-p $DISPLAY_PRIMARIES"
endif
set xiargs=""
set i=1
set firstarg=0
while ( $i <= $#argv && ! $firstarg )
	@ i1= $i + 1
	switch ( $argv[$i] )
	case -ge*:
	case -di*:
	case -g:
	case -c:
	case -e:
		if ( $i1 > $#argv ) goto notenough
		set xiargs=($xiargs $argv[$i] $argv[$i1])
		@ i1++
		breaksw
	case -d:
	case -b:
	case -m:
	case -f:
	case -s:
	case =*:
	case -o*:
		set xiargs=($xiargs $argv[$i])
		breaksw
	case -p:
		set popt="-p"
		while ( $i1 <= $i + 8 )
			if ( $i1 > $#argv ) goto notenough
			set popt="$popt $argv[$i1]"
			@ i1++
		end
		breaksw
	case -*:
		echo "Unknown option: $argv[$i]"
		exit 1
	default:
		set firstarg=$i
		breaksw
	endsw
	set i=$i1
end
onintr quit
if ( ! $firstarg ) then
	set rgbfiles=/usr/tmp/stdin.$$
	ra_xyze -r -u $popt > /usr/tmp/stdin.$$
	if ( $status ) goto quit
else
	set rgbfiles=""
	set i=$firstarg
	while ( $i <= $#argv )
		set rgbfiles=($rgbfiles /usr/tmp/$argv[$i]:t.$$)
		ra_xyze -r -u $popt $argv[$i] /usr/tmp/$argv[$i]:t.$$
		if ( $status ) goto quit
		@ i++
	end
endif
ximage $xiargs $rgbfiles
quit:
	rm -f $rgbfiles
	exit 0
notenough:
	echo "Missing arguments for $argv[$i] option"
	exit 1
