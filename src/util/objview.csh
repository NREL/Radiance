#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Make a nice view of an object
# Arguments are scene input files
#
set tmpdir=/usr/tmp
set octree=$tmpdir/ov$$.oct
set tmpfiles="$octree"
if ( ! $?RVIEW ) set RVIEW="rview"
onintr quit

oconv - $argv[*]:q > $octree <<_EOF_
void glow dim 0 0 4 .1 .1 .15 0
dim source background 0 0 4 0 0 1 360
void light bright 0 0 3 1000 1000 1000
bright source sun1 0 0 4 1 .2 1 5
bright source sun2 0 0 4 .3 1 1 5
bright source sun3 0 0 4 -1 -.7 1 5
_EOF_

set vp=`getinfo -d <$octree|rcalc -e '$1=$1-3.5*$4;$2=$2-3.5*$4;$3=$3+2.5*$4'`

$RVIEW -av .2 .2 .2 -vp $vp -vd 1 1 -.5 -vv 15 -vh 15 $octree

quit:
rm -f $tmpfiles
exit 0
