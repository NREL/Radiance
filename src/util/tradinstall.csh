#!/bin/csh -f
# SCCSid "$SunId$ LBL"
# Install correct version of trad for wish or wish4.0
#
set instdir = $1
set libdir = $2

set TLIBFILES = ( *[a-z].tcl *.hlp trad.icon tclIndex )

set TDIFFS = (`ls | sed -n 's/3\.6\.tcl$//p'`)

foreach d ($path)
	if (-x $d/wish4.0) then
		set wishcom = $d/wish4.0
		break
	endif
end
if (! $?wishcom) then
	foreach d ($path)
		if (-x $d/wish) then
			set wishcom = "$d/wish -f"
			break
		endif
	end
	if (! $?wishcom) then
		echo "Cannot find wish executable in current path -- trad not installed."
		exit 1
	endif
	set oldwish
endif

echo "Installing trad using $wishcom"

sed -e "1s|/usr/local/bin/wish4\.0|$wishcom|" \
	-e "s|^set radlib .*|set radlib $libdir|" trad.wsh > $instdir/trad
if ($status) exit 1
chmod 755 $instdir/trad
(cd $libdir ; rm -f $TLIBFILES)
cp $TLIBFILES $libdir
if ($?oldwish) then
	foreach i ($TDIFFS)
		(cd $libdir ; rm -f $i.tcl)
		cp ${i}3.6.tcl $libdir/$i.tcl
	end
endif
