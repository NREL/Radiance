/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines to print mkillum objects
 */

#include  "mkillum.h"

#define  brt(col)	(.295*(col)[0] + .636*(col)[1] + .070*(col)[2])

char	DATORD[] = "RGB";		/* data ordering */
char	DATSUF[] = ".dat";		/* data file suffix */
char	DSTSUF[] = ".dist";		/* distribution suffix */
char	FNCFNM[] = "illum.cal";		/* function file name */


printobj(mod, obj)		/* print out an object */
char  *mod;
register OBJREC  *obj;
{
	register int  i;

	printf("\n%s %s %s", mod, ofun[obj->otype].funame, obj->oname);
	printf("\n%d", obj->oargs.nsargs);
	for (i = 0; i < obj->oargs.nsargs; i++)
		printf(" %s", obj->oargs.sarg[i]);
#ifdef  IARGS
	printf("\n%d", obj->oargs.niargs);
	for (i = 0; i < obj->oargs.niargs; i++)
		printf(" %d", obj->oargs.iarg[i]);
#else
	printf("\n0");
#endif
	printf("\n%d", obj->oargs.nfargs);
	for (i = 0; i < obj->oargs.nfargs; i++) {
		if (i%3 == 0)
			putchar('\n');
		printf(" %18.12g", obj->oargs.farg[i]);
	}
	putchar('\n');
}


char *
dfname(il, c)			/* return data file name */
struct illum_args  *il;
int  c;
{
	extern char  *getpath(), *strcpy();
	char  fname[MAXSTR];
	register char  *s;

	s = strcpy(fname, il->datafile);
	s += strlen(s);
	if (c) *s++ = c;
	if (il->dfnum > 0) {
		sprintf(s, "%d", il->dfnum);
		s += strlen(s);
	}
	strcpy(s, DATSUF);
	return(getpath(fname, NULL, 0));
}


FILE *
dfopen(il, c)			/* open data file */
register struct illum_args  *il;
int  c;
{
	char  *fn;
	FILE  *fp;
					/* get a usable file name */
	for (fn = dfname(il, c);
			!(il->flags & IL_DATCLB) && access(fn, F_OK) == 0;
			fn = dfname(il, c))
		il->dfnum++;
					/* open it for writing */
	if ((fp = fopen(fn, "w")) == NULL) {
		sprintf(errmsg, "cannot open data file \"%s\"", fn);
		error(SYSTEM, errmsg);
	}
	return(fp);
}


illumout(il, ob)		/* print illum object */
register struct illum_args  *il;
OBJREC  *ob;
{
	double  cout[3];

	printf("\n%s%s %s %s", il->matname, DSTSUF,
			ofun[il->flags&IL_LIGHT?MAT_LIGHT:MAT_ILLUM].funame,
			il->matname);
	if (il->flags & IL_LIGHT || !strcmp(il->altmat,VOIDID))
		printf("\n0");
	else
		printf("\n1 %s", il->altmat);
	if (il->flags & IL_COLAVG) {
		cout[0] = il->col[0];
		cout[1] = il->col[1];
		cout[2] = il->col[2];
	} else {
		cout[0] = cout[1] = cout[2] = brt(il->col);
	}
	if (il->flags & IL_LIGHT)
		printf("\n3 %f %f %f\n", cout[0], cout[1], cout[2]);
	else
		printf("\n4 %f %f %f 0\n", cout[0], cout[1], cout[2]);

	printobj(il->matname, ob);
}


flatout(il, da, n, m, u, v, w)		/* write hemispherical distribution */
struct illum_args  *il;
float  *da;
int  n, m;
FVECT  u, v, w;
{
	FILE  *dfp;
	int  i;

	average(il, da, n*m);
	if (il->flags & IL_COLDST) {
		printf("\n%s %s %s%s", VOIDID, ofun[PAT_CDATA].funame,
				il->matname, DSTSUF);
		printf("\n9 red green blue");
		for (i = 0; i < 3; i++) {
			dfp = dfopen(il, DATORD[i]);
			fprintf(dfp, "2\n1 0 %d\n0 %f %d\n", n, 2.*PI, m);
			colorout(i, da, n*m, 1./il->nsamps/il->col[i], dfp);
			fclose(dfp);
			printf(" %s", dfname(il, DATORD[i]));
		}
	} else {
		printf("\n%s %s %s%s", VOIDID, ofun[PAT_BDATA].funame,
				il->matname, DSTSUF);
		printf("\n5 noop");
		dfp = dfopen(il, 0);
		fprintf(dfp, "2\n1 0 %d\n0 %f %d\n", n, 2.*PI, m);
		brightout(da, n*m, 1./il->nsamps/brt(il->col), dfp);
		fclose(dfp);
		printf(" %s", dfname(il, 0));
	}
	printf("\n\t%s il_alth il_azih", FNCFNM);
	printf("\n0\n9\n");
	printf("\t%f\t%f\t%f\n", u[0], u[1], u[2]);
	printf("\t%f\t%f\t%f\n", v[0], v[1], v[2]);
	printf("\t%f\t%f\t%f\n", w[0], w[1], w[2]);
	il->dfnum++;
}


roundout(il, da, n, m)			/* write spherical distribution */
struct illum_args  *il;
float  *da;
int  n, m;
{
	FILE  *dfp;
	int  i;

	average(il, da, n*m);
	if (il->flags & IL_COLDST) {
		printf("\n%s %s %s%s", VOIDID, ofun[PAT_CDATA].funame,
				il->matname, DSTSUF);
		printf("\n9 red green blue");
		for (i = 0; i < 3; i++) {
			dfp = dfopen(il, DATORD[i]);
			fprintf(dfp, "2\n1 -1 %d\n0 %f %d\n", n, 2.*PI, m);
			colorout(i, da, n*m, 1./il->nsamps/il->col[i], dfp);
			fclose(dfp);
			printf(" %s", dfname(il, DATORD[i]));
		}
	} else {
		printf("\n%s %s %s%s", VOIDID, ofun[PAT_BDATA].funame,
				il->matname, DSTSUF);
		printf("\n5 noop");
		dfp = dfopen(il, 0);
		fprintf(dfp, "2\n1 -1 %d\n0 %f %d\n", n, 2.*PI, m);
		brightout(da, n*m, 1./il->nsamps/brt(il->col), dfp);
		fclose(dfp);
		printf(" %s", dfname(il, 0));
	}
	printf("\n\t%s il_alt il_azi", FNCFNM);
	printf("\n0\n0\n");
	il->dfnum++;
}


average(il, da, n)		/* compute average value for distribution */
register struct illum_args  *il;
register float  *da;
int  n;
{
	register int  i;

	il->col[0] = il->col[1] = il->col[2] = 0.;
	i = n;
	while (i-- > 0) {
		il->col[0] += *da++;
		il->col[1] += *da++;
		il->col[2] += *da++;
	}
	for (i = 0; i < 3; i++)
		il->col[i] /= (double)n*il->nsamps;
}


colorout(p, da, n, mult, fp)	/* put out color distribution data */
int  p;
register float  *da;
int  n;
double  mult;
FILE  *fp;
{
	register int  i;

	for (i = 0; i < n; i++) {
		if (i%6 == 0)
			putc('\n', fp);
		fprintf(fp, " %11e", mult*da[p]);
		da += 3;
	}
	putc('\n', fp);
}


brightout(da, n, mult, fp)	/* put out brightness distribution data */
register float  *da;
int  n;
double  mult;
FILE  *fp;
{
	register int  i;

	for (i = 0; i < n; i++) {
		if (i%6 == 0)
			putc('\n', fp);
		fprintf(fp, " %11e", mult*brt(da));
		da += 3;
	}
	putc('\n', fp);
}
