#!/bin/csh -f
# SCCSid "$SunId$ LBL"
#
# Create false color image with legend
#
set td=/usr/tmp/fc$$
onintr quit
set mult=470
set label=Nits
set scale=1000
set decades=0
set redv='1.6*v-.6'
set grnv='if(v-.375,1.6-1.6*v,8/3*v)'
set bluv='1-8/3*v'
set ndivs=8
set picture='-'
set cpict=
set legwidth=100
set legheight=200
while ($#argv > 0)
	switch ($argv[1])
	case -m:
		shift argv
		set mult="$argv[1]"
		breaksw
	case -s:
		shift argv
		set scale="$argv[1]"
		breaksw
	case -l:
		shift argv
		set label="$argv[1]"
		breaksw
	case -log:
		shift argv
		set decades=$argv[1]
		breaksw
	case -r:
		shift argv
		set redv="$argv[1]"
		breaksw
	case -g:
		shift argv
		set grnv="$argv[1]"
		breaksw
	case -b:
		shift argv
		set bluv="$argv[1]"
		breaksw
	case -i:
		shift argv
		set picture="$argv[1]"
		breaksw
	case -p:
		shift argv
		set cpict="$argv[1]"
		breaksw
	case -ip:
	case -pi:
		shift argv
		set picture="$argv[1]"
		set cpict="$argv[1]"
		breaksw
	case -cl:
		set docont=a
		breaksw
	case -cb:
		set docont=b
		breaksw
	case -n:
		shift argv
		set ndivs="$argv[1]"
		breaksw
	default:
		echo bad option "'$argv[1]'" >/dev/tty
		exit 1
	endsw
	shift argv
end
mkdir $td
cat > $td/pc0.cal <<_EOF_
scale : $scale ;
mult : $mult ;
ndivs : $ndivs ;

or(a,b) : if(a,a,b);
EPS : 1e-7;
neq(a,b) : if(a-b-EPS,1,b-a-EPS);
btwn(a,b) : if(a-x,-1,b-x);
clip(x) : if(x-1,1,if(x,x,0));
frac(x) : x - floor(x);
boundary(a,b) : neq(floor(ndivs*a),floor(ndivs*b));

isconta = or(boundary(vleft,vright),boundary(vabove,vbelow));
iscontb = if(btwn(0,v,1),btwn(.4,frac(ndivs*v),.6),-1); 

ro = if(in,clip($redv),ra);
go = if(in,clip($grnv),ga);
bo = if(in,clip($bluv),ba);

ra = 0;
ga = 0;
ba = 0;

in = 1;
_EOF_
cat > $td/pc1.cal <<_EOF_
norm : mult/scale/le(1);

v = map(li(1)*norm);

vleft = map(li(1,-1,0)*norm);
vright = map(li(1,1,0)*norm);
vabove = map(li(1,0,1)*norm);
vbelow = map(li(1,0,-1)*norm);

map(x) = x;

ra = ri(nfiles);
ga = gi(nfiles);
ba = bi(nfiles);
_EOF_
set pc0args=(-f $td/pc0.cal)
set pc1args=(-f $td/pc1.cal)
if ($?docont) then
	set pc0args=($pc0args -e "in=iscont$docont")
endif
if ("$cpict" == "") then
	set pc1args=($pc1args -e 'ra=0;ga=0;ba=0')
else if ("$cpict" == "$picture") then
	set cpict=
endif
if ($decades > 0) then
	set pc1args=($pc1args -e "map(x)=if(x-10^-$decades,log10(x)/$decades+1,0)")
	set imap="imap(y)=10^((y-1)*$decades)"
else
	set imap="imap(y)=y"
endif
pcomb $pc0args -e 'v=(y+.5)/yres;vleft=v;vright=v' \
		-e 'vbelow=(y-.5)/yres;vabove=(y+1.5)/yres' \
		-x $legwidth -y $legheight > $td/scol.pic
( echo $label; cnt $ndivs \
		| rcalc -e '$1='"($scale)*imap(($ndivs-.5-"'$1'")/$ndivs)" \
		-e "$imap" ) \
	| psign -cf 1 1 1 -cb 0 0 0 -h `ev "floor(2*$legheight/$ndivs+.5)"` \
	| pfilt -1 -x /2 -y /2 > $td/slab.pic
pcomb $pc0args $pc1args $picture $cpict \
	| pcompos $td/scol.pic 0 0 -t .2 $td/slab.pic 25 0 - $legwidth 0
quit:
rm -rf $td
