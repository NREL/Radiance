/* Copyright (c) 1989 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
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
#include  "fvect.h"

#define  XNAME		"X_"			/* x function name */
#define  YNAME		"Y_"			/* y function name */
#define  ZNAME		"Z_"			/* z function name */

#define  PI		3.14159265358979323846

#define  FTINY		1e-7

#define  pvect(p)	printf(vformat, (p)[0], (p)[1], (p)[2])

char  vformat[] = "%15.9g %15.9g %15.9g\n";
char  tsargs[] = "4 surf_dx surf_dy surf_dz surf.cal\n";
char  texname[] = "Phong";

int  smooth = 0;		/* apply smoothing? */

char  *modname, *surfname;

double  funvalue(), l_hermite(), argument(), fabs();

typedef struct {
	FVECT  p;	/* vertex position */
	FVECT  n;	/* average normal */
} POINT;


main(argc, argv)
int  argc;
char  *argv[];
{
	POINT  *row0, *row1, *row2, *rp;
	int  i, j, m, n;
	char  stmp[256];

	varset("PI", PI);
	funset("hermite", 5, l_hermite);

	if (argc < 8)
		goto userror;

	for (i = 8; i < argc; i++)
		if (!strcmp(argv[i], "-e"))
			scompile(NULL, argv[++i]);
		else if (!strcmp(argv[i], "-f"))
			fcompile(argv[++i]);
		else if (!strcmp(argv[i], "-s"))
			smooth++;
		else
			goto userror;

	modname = argv[1];
	surfname = argv[2];
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

	row0 = (POINT *)malloc((n+1)*sizeof(POINT));
	row1 = (POINT *)malloc((n+1)*sizeof(POINT));
	row2 = (POINT *)malloc((n+1)*sizeof(POINT));
	if (row0 == NULL || row1 == NULL || row2 == NULL) {
		fprintf(stderr, "%s: out of memory\n", argv[0]);
		quit(1);
	}
						/* print header */
	printhead(argc, argv);
						/* compute first two rows */
	comprow(0.0, row1, n);
	comprow(1.0/m, row2, n);
	compnorms(row1, row1, row2, n);
						/* for each row */
	for (i = 0; i < m; i++) {
						/* compute next row */
		rp = row0;
		row0 = row1;
		row1 = row2;
		row2 = rp;
		if (i+2 <= m) {
			comprow((double)(i+2)/m, row2, n);
			compnorms(row0, row1, row2, n);
		} else
			compnorms(row0, row1, row1, n);

		for (j = 0; j < n; j++) {
							/* put polygons */
			if ((i+j) & 1)
				putsquare(&row0[j], &row1[j],
						&row0[j+1], &row1[j+1]);
			else
				putsquare(&row1[j], &row1[j+1],
						&row0[j], &row0[j+1]);
		}
	}

	quit(0);

userror:
	fprintf(stderr, "Usage: %s material name ", argv[0]);
	fprintf(stderr, "x(s,t) y(s,t) z(s,t) m n [-s][-e expr][-f file]\n");
	quit(1);
}


putsquare(p0, p1, p2, p3)		/* put out a square */
POINT  *p0, *p1, *p2, *p3;
{
	static int  nout = 0;
	FVECT  norm[4];
	int  axis;
	FVECT  v1, v2, vc1, vc2;
	int  ok1, ok2;
					/* compute exact normals */
	fvsum(v1, p1->p, p0->p, -1.0);
	fvsum(v2, p2->p, p0->p, -1.0);
	fcross(vc1, v1, v2);
	ok1 = normalize(vc1) != 0.0;
	fvsum(v1, p2->p, p3->p, -1.0);
	fvsum(v2, p1->p, p3->p, -1.0);
	fcross(vc2, v1, v2);
	ok2 = normalize(vc2) != 0.0;
	if (!(ok1 | ok2))
		return;
					/* compute normal interpolation */
	axis = norminterp(norm, p0, p1, p2, p3);

					/* put out quadrilateral? */
	if (ok1 & ok2 && fdot(vc1,vc2) >= 1.0-FTINY*FTINY) {
		printf("\n%s ", modname);
		if (axis != -1) {
			printf("texfunc %s\n", texname);
			printf(tsargs);
			printf("0\n13\t%d\n", axis);
			pvect(norm[0]);
			pvect(norm[1]);
			pvect(norm[2]);
			fvsum(v1, norm[3], vc1, -0.5);
			fvsum(v1, v1, vc2, -0.5);
			pvect(v1);
			printf("\n%s ", texname);
		}
		printf("polygon %s.%d\n", surfname, ++nout);
		printf("0\n0\n12\n");
		pvect(p0->p);
		pvect(p1->p);
		pvect(p3->p);
		pvect(p2->p);
		return;
	}
					/* put out triangles? */
	if (ok1) {
		printf("\n%s ", modname);
		if (axis != -1) {
			printf("texfunc %s\n", texname);
			printf(tsargs);
			printf("0\n13\t%d\n", axis);
			pvect(norm[0]);
			pvect(norm[1]);
			pvect(norm[2]);
			fvsum(v1, norm[3], vc1, -1.0);
			pvect(v1);
			printf("\n%s ", texname);
		}
		printf("polygon %s.%d\n", surfname, ++nout);
		printf("0\n0\n9\n");
		pvect(p0->p);
		pvect(p1->p);
		pvect(p2->p);
	}
	if (ok2) {
		printf("\n%s ", modname);
		if (axis != -1) {
			printf("texfunc %s\n", texname);
			printf(tsargs);
			printf("0\n13\t%d\n", axis);
			pvect(norm[0]);
			pvect(norm[1]);
			pvect(norm[2]);
			fvsum(v2, norm[3], vc2, -1.0);
			pvect(v2);
			printf("\n%s ", texname);
		}
		printf("polygon %s.%d\n", surfname, ++nout);
		printf("0\n0\n9\n");
		pvect(p2->p);
		pvect(p1->p);
		pvect(p3->p);
	}
}


comprow(s, row, siz)			/* compute row of values */
double  s;
register POINT  *row;
int  siz;
{
	double  st[2], step;

	st[0] = s;
	st[1] = 0.0;
	step = 1.0 / siz;
	while (siz-- >= 0) {
		row->p[0] = funvalue(XNAME, 2, st);
		row->p[1] = funvalue(YNAME, 2, st);
		row->p[2] = funvalue(ZNAME, 2, st);
		row++;
		st[1] += step;
	}
}


compnorms(r0, r1, r2, siz)		/* compute row of averaged normals */
register POINT  *r0, *r1, *r2;
int  siz;
{
	FVECT  v1, v2, vc;

	if (!smooth)			/* not needed if no smoothing */
		return;
					/* compute first point */
	fvsum(v1, r2[0].p, r1[0].p, -1.0);
	fvsum(v2, r1[1].p, r1[0].p, -1.0);
	fcross(r1[0].n, v1, v2);
	fvsum(v1, r0[0].p, r1[0].p, -1.0);
	fcross(vc, v2, v1);
	fvsum(r1[0].n, r1[0].n, vc, 1.0);
	normalize(r1[0].n);
	r0++; r1++; r2++;
					/* compute middle points */
	while (--siz > 0) {
		fvsum(v1, r2[0].p, r1[0].p, -1.0);
		fvsum(v2, r1[1].p, r1[0].p, -1.0);
		fcross(r1[0].n, v1, v2);
		fvsum(v1, r0[0].p, r1[0].p, -1.0);
		fcross(vc, v2, v1);
		fvsum(r1[0].n, r1[0].n, vc, 1.0);
		fvsum(v2, r1[-1].p, r1[0].p, -1.0);
		fcross(vc, v1, v2);
		fvsum(r1[0].n, r1[0].n, vc, 1.0);
		fvsum(v1, r2[0].p, r1[0].p, -1.0);
		fcross(vc, v2, v1);
		fvsum(r1[0].n, r1[0].n, vc, 1.0);
		normalize(r1[0].n);
		r0++; r1++; r2++;
	}
					/* compute end point */
	fvsum(v1, r0[0].p, r1[0].p, -1.0);
	fvsum(v2, r1[-1].p, r1[0].p, -1.0);
	fcross(r1[0].n, v1, v2);
	fvsum(v1, r2[0].p, r1[0].p, -1.0);
	fcross(vc, v2, v1);
	fvsum(r1[0].n, r1[0].n, vc, 1.0);
	normalize(r1[0].n);
}


int
norminterp(resmat, p0, p1, p2, p3)	/* compute normal interpolation */
register FVECT  resmat[4];
POINT  *p0, *p1, *p2, *p3;
{
#define u  ((ax+1)%3)
#define v  ((ax+2)%3)

	register int  ax;
	double  eqnmat[4][4], solmat[4][4];
	FVECT  v1;
	register int  i, j;

	if (!smooth)			/* no interpolation if no smoothing */
		return(-1);
					/* find dominant axis */
	VCOPY(v1, p0->n);
	fvsum(v1, v1, p1->n, 1.0);
	fvsum(v1, v1, p2->n, 1.0);
	fvsum(v1, v1, p3->n, 1.0);
	ax = fabs(v1[0]) > fabs(v1[1]) ? 0 : 1;
	ax = fabs(v1[ax]) > fabs(v1[2]) ? ax : 2;
					/* assign equation matrix */
	eqnmat[0][0] = p0->p[u]*p0->p[v];
	eqnmat[0][1] = p0->p[u];
	eqnmat[0][2] = p0->p[v];
	eqnmat[0][3] = 1.0;
	eqnmat[1][0] = p1->p[u]*p1->p[v];
	eqnmat[1][1] = p1->p[u];
	eqnmat[1][2] = p1->p[v];
	eqnmat[1][3] = 1.0;
	eqnmat[2][0] = p2->p[u]*p2->p[v];
	eqnmat[2][1] = p2->p[u];
	eqnmat[2][2] = p2->p[v];
	eqnmat[2][3] = 1.0;
	eqnmat[3][0] = p3->p[u]*p3->p[v];
	eqnmat[3][1] = p3->p[u];
	eqnmat[3][2] = p3->p[v];
	eqnmat[3][3] = 1.0;
					/* invert matrix (solve system) */
	if (!invmat(solmat, eqnmat))
		return(-1);			/* no solution */
					/* compute result matrix */
	for (j = 0; j < 4; j++)
		for (i = 0; i < 3; i++)
			resmat[j][i] =	solmat[j][0]*p0->n[i] +
					solmat[j][1]*p1->n[i] +
					solmat[j][2]*p2->n[i] +
					solmat[j][3]*p3->n[i];
	return(ax);

#undef u
#undef v
}


static double  m4tmp[4][4];		/* for efficiency */

#define  copymat4(m4a,m4b)	bcopy((char *)m4b,(char *)m4a,sizeof(m4tmp))


setident4(m4)
double  m4[4][4];
{
	static double  ident[4][4] = {
		1.,0.,0.,0.,
		0.,1.,0.,0.,
		0.,0.,1.,0.,
		0.,0.,0.,1.,
	};
	copymat4(m4, ident);
}

/*
 * invmat - computes the inverse of mat into inverse.  Returns 1
 * if there exists an inverse, 0 otherwise.  It uses Gaussian Elimination
 * method.
 */

invmat(inverse,mat)
double mat[4][4],inverse[4][4];
{
#define SWAP(a,b,t) (t=a,a=b,b=t)

	register int i,j,k;
	register double temp;

	setident4(inverse);
	copymat4(m4tmp, mat);

	for(i = 0; i < 4; i++) {
		if(m4tmp[i][i] == 0) {    /* Pivot is zero */
			/* Look for a raw with pivot != 0 and swap raws */
			for(j = i + 1; j < 4; j++)
				if(m4tmp[j][i] != 0) {
					for( k = 0; k < 4; k++) {
						SWAP(m4tmp[i][k],m4tmp[j][k],temp);
						SWAP(inverse[i][k],inverse[j][k],temp);
						}
					break;
					}
			if(j == 4)	/* No replacing raw -> no inverse */
				return(0);
			}

		temp = m4tmp[i][i];
		for(k = 0; k < 4; k++) {
			m4tmp[i][k] /= temp;
			inverse[i][k] /= temp;
			}
		for(j = 0; j < 4; j++) {
			if(j != i) {
				temp = m4tmp[j][i];
				for(k = 0; k < 4; k++) {
					m4tmp[j][k] -= m4tmp[i][k]*temp;
					inverse[j][k] -= inverse[i][k]*temp;
					}
				}
			}
		}
	return(1);
#undef SWAP
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
