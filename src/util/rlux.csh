#!/bin/csh -f
# RCSid: $Id$
#
# Compute illuminance from ray origin and direction
#
if ( $#argv < 1 ) then
	echo "Usage: $0 [rtrace args] octree"
	exit 1
endif
rtrace -i+ -dv- -h- -x 1 $argv[*]:q | rcalc -e '$1=47.4*$1+120*$2+11.6*$3' -u
