#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Find scene dependencies in this directory and one level down
#
if ( ! $?RAYPATH ) then
	set RAYPATH=.:/usr/local/lib/ray
endif
onintr quit
rm -f EMPTY
echo -n > EMPTY
sleep 1
( ls $* | sed -e 's@^@/^@' -e 's@$@$/d@' ; echo '/^EMPTY$/,$d' ) > /tmp/sed$$
xform -e $* | rcalc -l -i 'instance $(name) ${n} $(ot) ' -o '$(ot)\
' | sort -u > /tmp/otf$$
foreach ot (`cat /tmp/otf$$`)
	unset libfile
	foreach d (`echo $RAYPATH | sed 's/:/ /g'`)
		if ( $d == . ) continue
		if ( -r $d/$ot ) set libfile
	end
	if ( ! $?libfile ) echo $ot
end
ls -tu | sed -f /tmp/sed$$ | sort
quit:
rm -f /tmp/sed$$ /tmp/otf$$ EMPTY
