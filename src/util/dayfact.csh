#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Interactive script to calculate daylight factors
#
set nofile="none"
set genskyf=$nofile
set octree=$nofile
set dfpict=$nofile
set ilpict=$nofile
set wporig=(0 0 0)
set wpsize=(1 1)
set rtargs=(-ab 1 -ad 256 -as 128 -aa .15 -av .1 .1 .1)

set maxres=64
set iltemp=/usr/tmp/il$$.pic
set tempfiles=($iltemp)

alias readvar 'echo -n Enter \!:1 "[$\!:1]: ";set ans="$<";if("$ans" != "")set \!:1="$ans"'

onintr quit

cat <<_EOF_
			DAYLIGHT FACTOR CALCULATION

This script calculates daylight factors and/or illuminance levels on a
rectangular workplane and produces a contour plot from the result.  The
input is a Radiance scene description (and octree) and the output is one
or more color Radiance pictures.

_EOF_
readvar octree
if ( "$octree" == "$nofile" || ! -f "$octree" ) then
	echo "You must first create an octree with oconv"
	echo "before running this script."
	exit 1
endif
echo ""
echo "In what scene file is the gensky command located?"
readvar genskyf
if ( "$genskyf" == "$nofile" || ! -r "$genskyf" ) then
	echo "You will not be able to compute"
	echo "daylight factors without a gensky file"
else
	set extamb=`xform -e $genskyf|sed -n 's/^# Ground ambient level: //p'`
	echo extamb = $extamb
endif
echo -n "Is the z-axis your zenith direction? "
if ( "$<" !~ [yY]* ) then
	echo "I'm sorry, you cannot use this script"
	exit 1
endif
echo "What is the origin (smallest x y z coordinates) of the workplane?"
readvar wporig
set wporig=($wporig)
echo "What is the x and y size (width and length) of the workplane?"
readvar wpsize
set wpsize=($wpsize)
set wpres=(`rcalc -n -e '$1=if(l,'"floor($maxres*$wpsize[1]/$wpsize[2]),$maxres);"'$2=if(l,'"$maxres,floor($maxres*$wpsize[2]/$wpsize[1]));l=$wpsize[2]-$wpsize[1]"`)
set rtargs=($rtargs -ar `getinfo -d<$octree|rcalc -e '$1=floor(16*$4/'"($wpsize[1]+$wpsize[2]))"`)
echo "What calculation options do you want to give to rtrace?"
echo "(It is very important to set the -a? options correctly.)"
readvar rtargs
echo "Illuminance contour picture if you want one"
readvar ilpict
if ( $?extamb ) then
	echo "Daylight factor contour picture if you want one"
	readvar dfpict
endif
if ( "$ilpict" == "$nofile" && "$dfpict" == "$nofile" ) then
	echo "Since you don't want any output, I guess we're done."
	exit 0
endif
echo "Starting rtrace calculation -- this will take some time..."
cnt $wpres[2] $wpres[1] \
	| rcalc -e '$1=($2+.5)/'"$wpres[1]*$wpsize[1]+$wporig[1]" \
		-e '$2=(1-($1+.5)/'"$wpres[2])*$wpsize[2]+$wporig[2]" \
		-e '$3='"$wporig[3]" -e '$4=0;$5=0;$6=1' \
	| rtrace $rtargs -I -ov -faf $octree \
	| pvalue -r -x $wpres[1] -y $wpres[2] -df \
	| pfilt -h 20 -n 0 -x 256 -y 256 -p 1 -r 1 > $iltemp
set maxval=`getinfo < $iltemp | rcalc -i 'EXPOSURE=${e}' -e '$1=3/e'`
if ( "$ilpict" != "$nofile" ) then
	echo "Making illuminance contour picture $ilpict..."
	falsecolor -s "$maxval*470" -l Lux -log 2 -cl -pi $iltemp > $ilpict
endif
if ( "$dfpict" != "$nofile" ) then
	echo "Making daylight factor contour picture $dfpict..."
	falsecolor -s "$maxval/$extamb" -m "1/$extamb" -l DF \
		-log 2 -cl -pi $iltemp > $dfpict
endif
echo "Done."
quit:
	rm -f $tempfiles
