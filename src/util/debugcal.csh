#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Script to debug cal files for Radiance
#
# Takes octree and rcalc options as arguments.
# One of the rcalc options should be -f calfile.
# Note that the material arguments A1, A2, etc must also be given.
# Input is a ray origin and direction, such as that produced by ximage.
#
if ( $#argv < 2 ) then
	echo "Usage: $0 octree rcalc options..."
	exit 1
endif
if ( ! $?RAYPATH ) then
	set RAYPATH=.:/usr/local/lib/ray
endif
set initfile=
foreach d (`echo $RAYPATH | sed 's/:/ /g'`)
	if ( -r $d/rayinit.cal ) then
		set initfile="-f $d/rayinit.cal"
		break
	endif
end

rtrace -h -x 1 -odnp $1 | rcalc -e 'Dx=$1;Dy=$2;Dz=$3' \
		-e 'Nx=$4;Ny=$5;Nz=$6;Px=$7;Py=$8;Pz=$9' \
		-e 'Rdot=-Dx*Nx-Dy*Ny-Dz*Nz' $initfile $argv[2-]:q
