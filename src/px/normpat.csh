#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Normalize a picture to make it suitable as a tiling pattern.
# This script reduces the image and removes the lowest frequencies.
#
foreach f ($*)
	pfilt -e 2 -x 128 -y 128 -p 1 $f > np$$
	set resolu=`getinfo -d < np$$ | sed 's/-Y \([0-9]*\) +X \([0-9]*\)/\2 \1/'`
	set coef=`pfilt -1 -x 32 -y 32 np$$ | pvalue -h -b | rcalc -e '$1=2*$3*cos(wx);$2=2*$3*cos(wy);$3=2*$3*sin(wx);$4=2*$3*sin(wy);$5=4*$3*cos(wx)*cos(wy);$6=4*$3*cos(wx)*sin(wy);$7=4*$3*sin(wx)*cos(wy);$8=4*$3*sin(wx)*sin(wy);' -e 'wx=2*PI/32*$1;wy=2*PI/32*$2' | total -m`
	pcomb -e 'ro=ri(1)*f;go=gi(1)*f;bo=bi(1)*f;f=1-fc-fs-f0-f1' \
		-e "fc=$coef[1]*cos(wx)+$coef[2]*cos(wy)" \
		-e "fs=$coef[3]*sin(wx)+$coef[4]*sin(wy)" \
		-e "f0=$coef[5]*cos(wx)*cos(wy)+$coef[6]*cos(wx)*sin(wy)" \
		-e "f1=$coef[7]*sin(wx)*cos(wy)+$coef[8]*sin(wx)*sin(wy)" \
		-e "wx=2*3.1416/$resolu[1]*x;wy=2*3.1416/$resolu[2]*y" \
		np$$ > $f
	rm np$$
end
