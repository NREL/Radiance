/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  gensky.c - program to generate sky functions.
 *		Our zenith is along the Z-axis, the X-axis
 *		points east, and the Y-axis points north.
 *		Radiance is in watts/steradian/sq. meter.
 *
 *     3/26/86
 */

#include  <stdio.h>

#include  <math.h>

#include  "color.h"

#ifndef atof
extern double  atof();
#endif
extern char  *strcpy(), *strcat(), *malloc();
extern double  stadj(), sdec(), sazi(), salt();

#define  PI		3.141592654

#define  DOT(v1,v2)	(v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])

double  normsc();
					/* sun calculation constants */
extern double  s_latitude;
extern double  s_longitude;
extern double  s_meridian;
					/* required values */
int  month, day;
double  hour;
					/* default values */
int  cloudy = 0;
int  dosun = 1;
double  zenithbr = -1.0;
double  turbidity = 2.75;
double  gprefl = 0.2;
					/* computed values */
double  sundir[3];
double  groundbr;
double  F2;
double  solarbr;

char  *progname;
char  errmsg[128];


main(argc, argv)
int  argc;
char  *argv[];
{
	extern double  fabs();
	int  i;

	progname = argv[0];
	if (argc == 2 && !strcmp(argv[1], "-defaults")) {
		printdefaults();
		exit(0);
	}
	if (argc < 4)
		userror("arg count");
	month = atoi(argv[1]);
	day = atoi(argv[2]);
	hour = atof(argv[3]);
	for (i = 4; i < argc; i++)
		if (argv[i][0] == '-' || argv[i][0] == '+')
			switch (argv[i][1]) {
			case 's':
				cloudy = 0;
				dosun = argv[i][0] == '+';
				break;
			case 'c':
				cloudy = 1;
				dosun = 0;
				break;
			case 't':
				turbidity = atof(argv[++i]);
				break;
			case 'b':
				zenithbr = atof(argv[++i]);
				break;
			case 'g':
				gprefl = atof(argv[++i]);
				break;
			case 'a':
				s_latitude = atof(argv[++i]) * (PI/180);
				break;
			case 'o':
				s_longitude = atof(argv[++i]) * (PI/180);
				break;
			case 'm':
				s_meridian = atof(argv[++i]) * (PI/180);
				break;
			default:
				sprintf(errmsg, "unknown option: %s", argv[i]);
				userror(errmsg);
			}
		else
			userror("bad option");

	if (fabs(s_meridian-s_longitude) > 30*PI/180)
		fprintf(stderr,
	"%s: warning: %.1f hours btwn. standard meridian and longitude\n",
			progname, (s_longitude-s_meridian)*12/PI);

	printhead(argc, argv);

	computesky();
	printsky();
}


computesky()			/* compute sky parameters */
{
	int  jd;
	double  sd, st;
	double  altitude, azimuth;
					/* compute solar direction */
	jd = jdate(month, day);			/* Julian date */
	sd = sdec(jd);				/* solar declination */
	st = hour + stadj(jd);			/* solar time */
	altitude = salt(sd, st);
	azimuth = sazi(sd, st);
	sundir[0] = -sin(azimuth)*cos(altitude);
	sundir[1] = -cos(azimuth)*cos(altitude);
	sundir[2] = sin(altitude);

					/* Compute zenith brightness */
	if (zenithbr <= 0.0)
		if (cloudy) {
			zenithbr = 8.6*sundir[2] + .123;
			zenithbr *= 1000.0/SKYEFFICACY;
		} else {
			zenithbr = (1.376*turbidity-1.81)*tan(altitude)+0.38;
			zenithbr *= 1000.0/SKYEFFICACY;
		}
	if (zenithbr < 0.0)
		zenithbr = 0.0;
					/* Compute horizontal radiance */
	if (cloudy) {
		groundbr = zenithbr*0.777778;
		printf("# Ground ambient level: %f\n", groundbr);
	} else {
		F2 = 0.274*(0.91 + 10.0*exp(-3.0*(PI/2.0-altitude)) +
				0.45*sundir[2]*sundir[2]);
		groundbr = zenithbr*normsc(PI/2.0-altitude)/F2/PI;
		printf("# Ground ambient level: %f\n", groundbr);
		if (sundir[2] > 0.0) {
			if (sundir[2] > .16)
				solarbr = (1.5e9/SUNEFFICACY) *
					(1.147 - .147/sundir[2]);
			else
				solarbr = 1.5e9/SUNEFFICACY*(1.147-.147/.16);
			groundbr += solarbr*6e-5*sundir[2]/PI;
		} else
			dosun = 0;
	}
	groundbr *= gprefl;
}


printsky()			/* print out sky */
{
	if (dosun) {
		printf("\nvoid light solar\n");
		printf("0\n0\n");
		printf("3 %.2e %.2e %.2e\n", solarbr, solarbr, solarbr);
		printf("\nsolar source sun\n");
		printf("0\n0\n");
		printf("4 %f %f %f 0.5\n", sundir[0], sundir[1], sundir[2]);
	}
	
	printf("\nvoid brightfunc skyfunc\n");
	printf("2 skybright skybright.cal\n");
	printf("0\n");
	if (cloudy)
		printf("3 1 %.2e %.2e\n", zenithbr, groundbr);
	else
		printf("7 -1 %.2e %.2e %.2e %f %f %f\n", zenithbr, groundbr,
				F2, sundir[0], sundir[1], sundir[2]);
}


printdefaults()			/* print default values */
{
	if (cloudy)
		printf("-c\t\t\t\t# Cloudy sky\n");
	else if (dosun)
		printf("+s\t\t\t\t# Sunny sky with sun\n");
	else
		printf("-s\t\t\t\t# Sunny sky without sun\n");
	printf("-g %f\t\t\t# Ground plane reflectance\n", gprefl);
	if (zenithbr > 0.0)
		printf("-b %f\t\t\t# Zenith radiance (watts/ster/m2\n", zenithbr);
	else
		printf("-t %f\t\t\t# Atmospheric turbidity\n", turbidity);
	printf("-a %f\t\t\t# Site latitude (degrees)\n", s_latitude*(180/PI));
	printf("-o %f\t\t\t# Site longitude (degrees)\n", s_longitude*(180/PI));
	printf("-m %f\t\t\t# Standard meridian (degrees)\n", s_meridian*(180/PI));
}


userror(msg)			/* print usage error and quit */
char  *msg;
{
	if (msg != NULL)
		fprintf(stderr, "%s: Use error - %s\n", progname, msg);
	fprintf(stderr, "Usage: %s month day hour [options]\n", progname);
	fprintf(stderr, "   Or: %s -defaults\n", progname);
	exit(1);
}


double
normsc(theta)			/* compute normalization factor (E0*F2/L0) */
double  theta;
{
	static double  nf[5] = {2.766521, 0.547665,
				-0.369832, 0.009237, 0.059229};
	double  x, nsc;
	register int  i;
					/* polynomial approximation */
	x = (theta - PI/4.0)/(PI/4.0);
	nsc = nf[4];
	for (i = 3; i >= 0; i--)
		nsc = nsc*x + nf[i];

	return(nsc);
}


printhead(ac, av)		/* print command header */
register int  ac;
register char  **av;
{
	putchar('#');
	while (ac--) {
		putchar(' ');
		fputs(*av++, stdout);
	}
	putchar('\n');
}
