#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Compute illuminance from ray origin and direction
#
if ( $#argv < 1 ) then
	echo "Usage: $0 [rtrace args] octree"
	exit 1
endif
rtrace -i+ -dv- -h- -x 1 $argv[*]:q | rcalc -e '$1=47.1*$1+117.2*$2+14.7*$3' -u
