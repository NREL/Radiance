#!/bin/csh -fe
# RCSid: $Id$
#
# Prepare Instant View directory
#
if ( $#argv != 1 && $#argv != 2 ) then
	echo Usage: $0 radfile '[directory]'
	exit 1
endif
set radfile=$1
if ( $#argv == 2 ) then
	set dir=$2
else
	set dir=$radfile:r.ivd
endif
if ( ! -d $dir ) then
	mkdir $dir
endif
foreach vw ( xl Xl yl Yl zl Zl )
	rad -w -v $vw $radfile PIC=$dir/pvw QUA=L RES=768 \
			"render=-z $dir/pvw_$vw.z -pj 0"
end
