#!/usr/bin/perl -w
# RCSid $Id$
#
# Generate Reinhart vector for a given sky description
#
#	G. Ward
#
use strict;
my $windoz = ($^O eq "MSWin32" or $^O eq "MSWin64");
my @skycolor = (0.960, 1.004, 1.118);
my $mf = 4;
my $dosky = 1;
my $headout = 1;
my @origARGV = @ARGV;
while ($#ARGV >= 0) {
	if ("$ARGV[0]" eq "-c") {
		@skycolor = @ARGV[1..3];
		shift @ARGV for (1..3);
	} elsif ("$ARGV[0]" eq "-m") {
		$mf = $ARGV[1];
		shift @ARGV;
	} elsif ("$ARGV[0]" eq "-d") {
		$dosky = 0;
	} elsif ("$ARGV[0]" eq "-h") {
		$headout = 0;
	}
	shift @ARGV;
}
# Load sky description into line array, separating sun if one
my @skydesc;
my $lightline;
my @sunval;
my $sunline;
my $skyOK = 0;
my $srcmod;	# putting this inside loop breaks code(?!)
while (<>) {
	push @skydesc, $_;
	if (/^\w+\s+light\s+/) {
		s/\s*$//; s/^.*\s//;
		$srcmod = $_;
		$lightline = $#skydesc;
	} elsif (defined($srcmod) && /^($srcmod)\s+source\s/) {
		@sunval = split(' ', $skydesc[$lightline + 3]);
		shift @sunval;
		$sunline = $#skydesc;
	} elsif (/\sskyfunc\s*$/) {
		$skyOK = 1;
	}
}
die "Bad sky description!\n" if (! $skyOK);
# Strip out the solar source if present
my @sundir;
if (defined $sunline) {
	@sundir = split(' ', $skydesc[$sunline + 3]);
	shift @sundir;
	undef @sundir if ($sundir[2] <= 0);
	splice(@skydesc, $sunline, 5);
}
# Reinhart sky sample generator
my $rhcal = 'DEGREE : PI/180;' .
	'x1 = .5; x2 = .5;' .
	'alpha : 90/(MF*7 + .5);' .
	'tnaz(r) : select(r, 30, 30, 24, 24, 18, 12, 6);' .
	'rnaz(r) : if(r-(7*MF-.5), 1, MF*tnaz(floor((r+.5)/MF) + 1));' .
	'raccum(r) : if(r-.5, rnaz(r-1) + raccum(r-1), 0);' .
	'RowMax : 7*MF + 1;' .
	'Rmax : raccum(RowMax);' .
	'Rfindrow(r, rem) : if(rem-rnaz(r)-.5, Rfindrow(r+1, rem-rnaz(r)), r);' .
	'Rrow = if(Rbin-(Rmax-.5), RowMax-1, Rfindrow(0, Rbin));' .
	'Rcol = Rbin - raccum(Rrow) - 1;' .
	'Razi_width = 2*PI / rnaz(Rrow);' .
	'RAH : alpha*DEGREE;' .
	'Razi = if(Rbin-.5, (Rcol + x2 - .5)*Razi_width, 2*PI*x2);' .
	'Ralt = if(Rbin-.5, (Rrow + x1)*RAH, asin(-x1));' .
	'Romega = if(.5-Rbin, 2*PI, if(Rmax-.5-Rbin, ' .
	'	Razi_width*(sin(RAH*(Rrow+1)) - sin(RAH*Rrow)),' .
	'	2*PI*(1 - cos(RAH/2)) ) );' .
	'cos_ralt = cos(Ralt);' .
	'Dx = sin(Razi)*cos_ralt;' .
	'Dy = cos(Razi)*cos_ralt;' .
	'Dz = sin(Ralt);' ;
my ($nbins, $octree, $tregcommand, $suncmd);
if ($windoz) {
	$nbins = `rcalc -n -e MF:$mf -e \"$rhcal\" -e \"\$1=Rmax+1\"`;
	chomp $nbins;
	$octree = "gtv$$.oct";
	$tregcommand = "cnt $nbins 16 | rcalc -e MF:$mf -e \"$rhcal\" " .
		q{-e "Rbin=$1;x1=rand(recno*.37-5.3);x2=rand(recno*-1.47+.86)" } .
		q{-e "$1=0;$2=0;$3=0;$4=Dx;$5=Dy;$6=Dz" } .
		"| rtrace -h -ab 0 -w $octree | total -16 -m";
	if (@sundir) {
		$suncmd = "cnt " . ($nbins-1) .
			" | rcalc -e MF:$mf -e \"$rhcal\" -e Rbin=recno " .
			"-e \"dot=Dx*$sundir[0] + Dy*$sundir[1] + Dz*$sundir[2]\" " .
			"-e \"cond=dot-.866\" " .
			q{-e "$1=if(1-dot,acos(dot),0);$2=Romega;$3=recno" };
	}
} else {
	$nbins = `rcalc -n -e MF:$mf -e \'$rhcal\' -e \'\$1=Rmax+1\'`;
	chomp $nbins;
	$octree = "/tmp/gtv$$.oct";
	$tregcommand = "cnt $nbins 16 | rcalc -of -e MF:$mf -e '$rhcal' " .
		q{-e 'Rbin=$1;x1=rand(recno*.37-5.3);x2=rand(recno*-1.47+.86)' } .
		q{-e '$1=0;$2=0;$3=0;$4=Dx;$5=Dy;$6=Dz' } .
		"| rtrace -h -ff -ab 0 -w $octree | total -if3 -16 -m";
	if (@sundir) {
		$suncmd = "cnt " . ($nbins-1) .
			" | rcalc -e MF:$mf -e '$rhcal' -e Rbin=recno " .
			"-e 'dot=Dx*$sundir[0] + Dy*$sundir[1] + Dz*$sundir[2]' " .
			"-e 'cond=dot-.866' " .
			q{-e '$1=if(1-dot,acos(dot),0);$2=Romega;$3=recno' };
	}
}
my @tregval;
if ($dosky) {
	# Create octree for rtrace
	open OCONV, "| oconv - > $octree";
	print OCONV @skydesc;
	print OCONV "skyfunc glow skyglow 0 0 4 @skycolor 0\n";
	print OCONV "skyglow source sky 0 0 4 0 0 1 360\n";
	close OCONV;
	# Run rtrace and average output for every 16 samples
	@tregval = `$tregcommand`;
	unlink $octree;
} else {
	push @tregval, "0\t0\t0\n" for (1..$nbins);
}
# Find closest 3 patches to sun and divvy up direct solar contribution
sub numSort1 {
	my @a1 = split("\t", $a);
	my @b1 = split("\t", $b);
	return ($a1[0] <=> $b1[0]);
}
if (@sundir) {
	my $somega = ($sundir[3]/360)**2 * 3.141592654**3;
	my @bestdir = `$suncmd`;
	@bestdir = sort numSort1 @bestdir;
	my (@ang, @dom, @ndx);
	my $wtot = 0;
	for my $i (0..2) {
		($ang[$i], $dom[$i], $ndx[$i]) = split(' ', $bestdir[$i]);
		$wtot += 1./($ang[$i]+.02);
	}
	for my $i (0..2) {
		my $wt = 1./($ang[$i]+.02)/$wtot * $somega / $dom[$i];
		my @scolor = split(' ', $tregval[$ndx[$i]]);
		for my $j (0..2) { $scolor[$j] += $wt * $sunval[$j]; }
		$tregval[$ndx[$i]] = "$scolor[0]\t$scolor[1]\t$scolor[2]\n";
	}
}
# Output header if requested
if ($headout) {
	print "#?RADIANCE\n";
	print "genskyvec @origARGV\n";
	print "NROWS=", $#tregval+1, "\n";
	print "NCOLS=1\nNCOMP=3\n";
	print "FORMAT=ascii\n";
	print "\n";
}
# Output our final vector
print @tregval;
