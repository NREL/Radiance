#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Make a nice view of an object
# Standard input is description made to fit in unit cube in positive quadrant
#
set objdir=/usr/local/lib/ray/lib
set tmpdir=/usr/tmp
set vw="-vp 3 3 3 -vd -1 -1 -1 -vh 20 -vv 20"
set rview="rview -av .2 .2 .2 $vw $*"
set octree=$tmpdir/ov$$.oct
set tmpfiles="$octree"
onintr quit

oconv $objdir/testroom - > $octree
$rview $octree

quit:
rm -f $tmpfiles
exit 0
