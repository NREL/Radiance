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
set fcopts=($*)
set wporig=(0 0 0)
set wpsize=(1 1)
set rtargs=(-ab 1 -ad 256 -as 128 -aa .15 -av .1 .1 .1)
set maxres=128

alias readvar 'echo -n Enter \!:1 "[$\!:1]: ";set ans="$<";if("$ans" != "")set \!:1="$ans"'

cat <<_EOF_
			DAYLIGHT FACTOR CALCULATION

This script calculates daylight factors and/or illuminance levels on a
rectangular workplane and produces a contour plot from the result.  The
input is a Radiance scene description (and octree) and the output is one
or more color Radiance pictures.

_EOF_
readvar octree
if ( "$octree" == "$nofile" || ! -f "$octree" ) then
	echo "You must first create an octree with"
	echo "oconv before running this script."
	exit 1
endif
set title="$octree:r"
echo "In what scene file is the gensky command located?"
readvar genskyf
if ( "$genskyf" == "$nofile" || ! -r "$genskyf" ) then
	echo "You will not be able to compute daylight"
	echo "factors since there is no gensky file."
else
	set title=$title\ `sed -n 's/^.*\<gensky  *\([0-9][0-9]*  *[0-9][0-9]*  *[0-9][0-9.]*\).*$/\1/p' $genskyf`
	set extamb=`xform -e $genskyf|sed -n 's/^# Ground ambient level: //p'`
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
echo "(It is very important to set the -a* options correctly.)"
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
echo "Title for output picture"
readvar title
cat <<'_EOF_' > $sctemp
set iltemp=/usr/tmp/il$$.pic
set sctemp=/usr/tmp/sc$$.csh
set tltemp=/usr/tmp/tl$$.pic
set tempfiles=($iltemp $sctemp $tltemp)
echo "Your dayfact job is finished."
echo "Please check for error messages below."
echo ""
set echo
cnt $wpres[2] $wpres[1] \
	| rcalc -e '$1=($2+.5)/'"$wpres[1]*$wpsize[1]+$wporig[1]" \
		-e '$2=(1-($1+.5)/'"$wpres[2])*$wpsize[2]+$wporig[2]" \
		-e '$3='"$wporig[3]" -e '$4=0;$5=0;$6=1' \
	| rtrace $rtargs -I -ov -faf $octree \
	| pvalue -r -x $wpres[1] -y $wpres[2] -df \
	| pfilt -h 20 -n 0 -x 350 -y 350 -p 1 -r 1 > $iltemp
set maxval=`getinfo < $iltemp | rcalc -i 'EXPOSURE=${e}' -e '$1=3/e'`
psign -h 50 " $title " | pfilt -1 -x /2 -y /2 > $tltemp
'_EOF_'
if ( "$ilpict" != "$nofile" ) then
	echo 'falsecolor -cb -l Lux $fcopts \\
		-s "$maxval*470" -m 470 -pi $iltemp \\
		| pcompos -a 1 - $tltemp > $ilpict' >> $sctemp
endif
if ( "$dfpict" != "$nofile" ) then
	echo 'falsecolor -cb -l DF $fcopts \\
		-s "$maxval/$extamb" -m "1/$extamb" -pi $iltemp \\
		| pcompos -a 1 - $tltemp > $dfpict' >> $sctemp
endif
echo 'rm -f $tempfiles' >> $sctemp
(source $sctemp) |& mail `whoami` &
echo "Your job is started in the background."
echo "You will be notified by mail when it is done."
