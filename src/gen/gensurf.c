/*

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif
 *  gensurf.c - program to generate functional surfaces
 *
 *	Parametric functions x(s,t), y(s,t) and z(s,t)
 *  specify the surface, which is tesselated into an m by n
 *  array of paired triangles.
 *	The surface normal is defined by the right hand
 *  rule applied to (s,t).
 *
 *	4/3/87
 */

#include  <stdio.h>

#define  XNAME		"X_"			/* x function name */
#define  YNAME		"Y_"			/* y function name */
#define  ZNAME		"Z_"			/* z function name */

#define  PI		3.14159265358979323846

#define  FTINY		1e-7

#define  vertex(p)	printf(vformat, (p)[0], (p)[1], (p)[2])

char  vformat[] = "%15.9g %15.9g %15.9g\n";

double  funvalue(), dist2(), fdot(), l_hermite(), argument();


main(argc, argv)
int  argc;
char  *argv[];
{
	static double  *xyz[4];
	double  *row0, *row1, *dp;
	double  v1[3], v2[3], vc1[3], vc2[3];
	double  a1, a2;
	int  i, j, m, n;
	char  stmp[256];
	double  d;
	register int  k;

	varset("PI", PI);
	funset("hermite", 5, l_hermite);

	if (argc < 8)
		goto userror;

	for (i = 8; i < argc; i++)
		if (!strcmp(argv[i], "-e"))
			scompile(NULL, argv[++i]);
		else if (!strcmp(argv[i], "-f"))
			fcompile(argv[++i]);
		else
			goto userror;

	sprintf(stmp, "%s(s,t)=%s;", XNAME, argv[3]);
	scompile(NULL, stmp);
	sprintf(stmp, "%s(s,t)=%s;", YNAME, argv[4]);
	scompile(NULL, stmp);
	sprintf(stmp, "%s(s,t)=%s;", ZNAME, argv[5]);
	scompile(NULL, stmp);
	m = atoi(argv[6]);
	n = atoi(argv[7]);
	if (m <= 0 || n <= 0)
		goto userror;

	row0 = (double *)malloc((n+1)*3*sizeof(double));
	row1 = (double *)malloc((n+1)*3*sizeof(double));
	if (row0 == NULL || row1 == NULL) {
		fprintf(stderr, "%s: out of memory\n", argv[0]);
		quit(1);
	}

	printhead(argc, argv);

	comprow(0.0, row1, n);			/* compute zeroeth row */

	for (i = 0; i < m; i++) {
						/* compute next row */
		dp = row0;
		row0 = row1;
		row1 = dp;
		comprow((double)(i+1)/m, row1, n);

		for (j = 0; j < n; j++) {
							/* get vertices */
			xyz[0] = row0 + 3*j;
			xyz[1] = row1 + 3*j;
			xyz[2] = xyz[0] + 3;
			xyz[3] = xyz[1] + 3;
							/* rotate vertices */
			if (dist2(xyz[0],xyz[3]) < dist2(xyz[1],xyz[2])-FTINY) {
				dp = xyz[0];
				xyz[0] = xyz[1];
				xyz[1] = xyz[3];
				xyz[3] = xyz[2];
				xyz[2] = dp;
			}
							/* get normals */
			for (k = 0; k < 3; k++) {
				v1[k] = xyz[1][k] - xyz[0][k];
				v2[k] = xyz[2][k] - xyz[0][k];
			}
			fcross(vc1, v1, v2);
			a1 = fdot(vc1, vc1);
			for (k = 0; k < 3; k++) {
				v1[k] = xyz[2][k] - xyz[3][k];
				v2[k] = xyz[1][k] - xyz[3][k];
			}
			fcross(vc2, v1, v2);
			a2 = fdot(vc2, vc2);
							/* check coplanar */
			if (a1 > FTINY*FTINY && a2 > FTINY*FTINY) {
				d = fdot(vc1, vc2);
				if (d*d/a1/a2 >= 1.0-FTINY*FTINY) {
					if (d > 0.0) {	/* coplanar */
						printf(
						"\n%s polygon %s.%d.%d\n",
						argv[1], argv[2], i+1, j+1);
						printf("0\n0\n12\n");
						vertex(xyz[0]);
						vertex(xyz[1]);
						vertex(xyz[3]);
						vertex(xyz[2]);
					}		/* else overlapped */
					continue;
				}			/* else bent */
			}
							/* check triangles */
			if (a1 > FTINY*FTINY) {
				printf("\n%s polygon %s.%da%d\n",
					argv[1], argv[2], i+1, j+1);
				printf("0\n0\n9\n");
				vertex(xyz[0]);
				vertex(xyz[1]);
				vertex(xyz[2]);
			}
			if (a2 > FTINY*FTINY) {
				printf("\n%s polygon %s.%db%d\n",
					argv[1], argv[2], i+1, j+1);
				printf("0\n0\n9\n");
				vertex(xyz[2]);
				vertex(xyz[1]);
				vertex(xyz[3]);
			}
		}
	}

	quit(0);

userror:
	fprintf(stderr, "Usage: %s material name ", argv[0]);
	fprintf(stderr, "x(s,t) y(s,t) z(s,t) m n [-e expr] [-f file]\n");
	quit(1);
}


comprow(s, row, siz)			/* compute row of values */
double  s;
register double  *row;
int  siz;
{
	double  st[2], step;

	st[0] = s;
	st[1] = 0.0;
	step = 1.0 / siz;
	while (siz-- >= 0) {
		*row++ = funvalue(XNAME, 2, st);
		*row++ = funvalue(YNAME, 2, st);
		*row++ = funvalue(ZNAME, 2, st);
		st[1] += step;
	}
}


eputs(msg)
char  *msg;
{
	fputs(msg, stderr);
}


wputs(msg)
char  *msg;
{
	eputs(msg);
}


quit(code)
{
	exit(code);
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


double
l_hermite()			
{
	double  t;
	
	t = argument(5);
	return(	argument(1)*((2.0*t-3.0)*t*t+1.0) +
		argument(2)*(-2.0*t+3.0)*t*t +
		argument(3)*((t-2.0)*t+1.0)*t +
		argument(4)*(t-1.0)*t*t );
}
