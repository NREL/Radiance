#!/bin/csh -f
# RCSid: $Id$
#
# Find scene dependencies in this directory
#
set es=1
onintr quit
rm -f EMPTY
echo -n > EMPTY
sleep 2
set sedf=`mktemp /tmp/sed.XXXXXX`
( ls $* | sed -e 's~/~\\/~g' -e 's@^@/^@' -e 's@$@$/d@' ; echo '/^EMPTY$/,$d' ) > $sedf
getbbox -w $* >/dev/null
set es=$status
if ( $es == 0 ) then
	sync
	sleep 2
	ls -tuL | sed -f $sedf | sort
endif
quit:
rm -f $sedf EMPTY
exit $es
