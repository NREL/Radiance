#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Normalize a pattern for tiling (-b option blends edges) by removing
# lowest frequencies from image and reducing to standard size (-r option)
#
set pf="pfilt -e 2"
while ($#argv > 0)
	switch ($argv[1])
	case -r:
		shift argv
		set pf="$pf -x $argv[1] -y $argv[1] -p 1"
		breaksw
	case -b:
		set blend
		breaksw
	case -v:
		set verb
		breaksw
	case -*:
		echo bad option $argv[1]
		exit 1
	default:
		goto dofiles
	endsw
	shift argv
end
dofiles:
onintr quit
set td=/usr/tmp/np$$
mkdir $td
set ha=$0
set ha=$ha:t
if ( $?blend ) then
	set ha="$ha -b"
else
	mknod $td/hf p
endif
foreach f ($*)
	if ( $?verb ) then
		echo $f\:
		echo adjusting exposure/size...
	endif
	$pf $f > $td/pf
	getinfo < $td/pf > $f
	ed - $f << _EOF_
i
$ha
.
w
q
_EOF_
	set resolu=`getinfo -d < $td/pf | sed 's/-Y \([0-9]*\) +X \([0-9]*\)/\2 \1/'`
	if ( $?verb ) then
		echo computing Fourier coefficients...
	endif
	set coef=`pfilt -1 -x 32 -y 32 $td/pf | pvalue -h -b | rcalc -e '$1=2*$3*cos(wx);$2=2*$3*cos(wy);$3=2*$3*sin(wx);$4=2*$3*sin(wy);$5=4*$3*cos(wx)*cos(wy);$6=4*$3*cos(wx)*sin(wy);$7=4*$3*sin(wx)*cos(wy);$8=4*$3*sin(wx)*sin(wy);' -e 'wx=2*PI/32*$1;wy=2*PI/32*$2' | total -m`
	if ( $?verb ) then
		echo "cosx cosy sinx siny"
		echo $coef[1-4]
		echo "cosx*cosy cosx*siny sinx*cosy sinx*siny"
		echo $coef[5-8]
		echo removing low frequencies...
	endif
	if ( ! $?blend ) then
		( getinfo - < $td/hf >> $f & ) > /dev/null
	endif
	pcomb -e 'ro=f*ri(1);go=f*gi(1);bo=f*bi(1);f=(i-fc-fs-f0-f1)/i' \
		-e 'i=.263*ri(1)+.655*gi(1)+.082*bi(1)' \
		-e "fc=$coef[1]*cos(wx)+$coef[2]*cos(wy)" \
		-e "fs=$coef[3]*sin(wx)+$coef[4]*sin(wy)" \
		-e "f0=$coef[5]*cos(wx)*cos(wy)+$coef[6]*cos(wx)*sin(wy)" \
		-e "f1=$coef[7]*sin(wx)*cos(wy)+$coef[8]*sin(wx)*sin(wy)" \
		-e "wx=2*3.1416/$resolu[1]*x;wy=2*3.1416/$resolu[2]*y" \
		$td/pf > $td/hf
	if ( $?blend ) then
		if ( $?verb ) then
			echo blending edges...
		endif
		@ mar= $resolu[1] - 3
		pcompos -x 3 $td/hf 0 0 > $td/left
		pcompos $td/hf -$mar 0 > $td/right
		pcomb -e 'ro=f(ri);go=f(gi);bo=f(bi)' \
			-e 'f(p)=(3-x)/7*p(1)+(4+x)/7*p(2)' \
			$td/right $td/left > $td/left.patch
		pcomb -e 'ro=f(ri);go=f(gi);bo=f(bi)' \
			-e 'f(p)=(1+x)/7*p(1)+(6-x)/7*p(2)' \
			$td/left $td/right > $td/right.patch
		pcompos $td/hf 0 0 $td/left.patch 0 0 $td/right.patch $mar 0 \
			> $td/hflr
		@ mar= $resolu[2] - 3
		pcompos -y 3 $td/hflr 0 0 > $td/bottom
		pcompos $td/hflr 0 -$mar > $td/top
		pcomb -e 'ro=f(ri);go=f(gi);bo=f(bi)' \
			-e 'f(p)=(3-y)/7*p(1)+(4+y)/7*p(2)' \
			$td/top $td/bottom > $td/bottom.patch
		pcomb -e 'ro=f(ri);go=f(gi);bo=f(bi)' \
			-e 'f(p)=(1+y)/7*p(1)+(6-y)/7*p(2)' \
			$td/bottom $td/top > $td/top.patch
		pcompos $td/hflr 0 0 $td/bottom.patch 0 0 $td/top.patch 0 $mar \
			| getinfo - >> $f
	endif
	if ( $?verb ) then
		echo $f done.
	endif
end
quit:
rm -rf $td
