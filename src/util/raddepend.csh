#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Find scene dependencies in this directory and one level down
#
onintr quit
( ls $* | sed -e 's@^@/^@' -e 's@$@$/d@' ; echo "/^com$$"'$/,$d' ) > com$$
xform -e $* > /dev/null
ls -tu * */* | sed -f com$$ | sort
quit:
rm -f com$$
