#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Find scene dependencies in this directory and one level down
#
set es=1
onintr quit
rm -f EMPTY
echo -n > EMPTY
sleep 1
( ls $* | sed -e 's@^@/^@' -e 's@$@$/d@' ; echo '/^EMPTY$/,$d' ) > /tmp/sed$$
getbbox -w $* >/dev/null
set es=$status
if ( $es == 0 ) then
	ls -tu | sed -f /tmp/sed$$ | sort
endif
quit:
rm -f /tmp/sed$$ EMPTY
exit $es
