/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert Radiance scene description to MGF
 */

#include <stdio.h>
#include <string.h>
#include "fvect.h"
#include "object.h"
#include "color.h"
#include "lookup.h"

int	o_face(), o_cone(), o_sphere(), o_ring(), o_cylinder();
int	o_instance(), o_source(), o_illum();
int	o_plastic(), o_metal(), o_glass(), o_mirror(), o_trans(), o_light();

extern void	free();
extern char	*malloc();

LUTAB	rmats = LU_SINIT(free,NULL);		/* defined material table */

LUTAB	rdispatch = LU_SINIT(NULL,NULL);	/* function dispatch table */

char	curmat[80];		/* current material */

double	unit_mult = 1.;		/* units multiplier */


main(argc, argv)
int	argc;
char	**argv;
{
	int	i;

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'd':			/* units */
			switch (argv[i][2]) {
			case 'm':			/* meters */
				unit_mult = 1.;
				break;
			case 'c':			/* centimeters */
				unit_mult = .01;
				break;
			case 'f':			/* feet */
				unit_mult = 12.*.0254;
				break;
			case 'i':			/* inches */
				unit_mult = .0254;
				break;
			default:
				goto unkopt;
			}
			break;
		}
	init();
	if (i >= argc)
		rad2mgf(NULL);
	else
		for ( ; i < argc; i++)
			rad2mgf(argv[i]);
	uninit();
	exit(0);
unkopt:
	fprintf(stderr, "Usage: %s [-d{m|c|f|i}] file ..\n", argv[0]);
	exit(1);
}


rad2mgf(inp)		/* convert a Radiance file to MGF */
char	*inp;
{
#define mod	buf
#define typ	(buf+128)
#define id	(buf+256)
#define alias	(buf+384)
	char	buf[512];
	FUNARGS	fa;
	register FILE	*fp;
	register int	c;

	if (inp == NULL) {
		inp = "the standard input";
		fp = stdin;
	} else if (inp[0] == '!') {
		if ((fp = popen(inp+1, "r")) == NULL) {
			fputs(inp, stderr);
			fputs(": cannot execute\n", stderr);
			exit(1);
		}
	} else if ((fp = fopen(inp, "r")) == NULL) {
		fputs(inp, stderr);
		fputs(": cannot open\n", stderr);
		exit(1);
	}
	printf("# Begin conversion from: %s\n", inp);
	while ((c = getc(fp)) != EOF)
		switch (c) {
		case ' ':		/* white space */
		case '\t':
		case '\n':
		case '\r':
		case '\f':
			break;
		case '#':		/* comment */
			if (fgets(buf, sizeof(buf), fp) != NULL)
				printf("# %s", buf);
			break;
		case '!':		/* inline command */
			ungetc(c, fp);
			fgetline(buf, sizeof(buf), fp);
			rad2mgf(buf);
			break;
		default:		/* Radiance primitive */
			ungetc(c, fp);
			if (fscanf(fp, "%s %s %s", mod, typ, id) != 3) {
				fputs(inp, stderr);
				fputs(": unexpected EOF\n", stderr);
				exit(1);
			}
			if (!strcmp(typ, "alias")) {
				strcpy(alias, "EOF");
				fscanf(fp, "%s", alias);
				newmat(id, alias);
			} else {
				if (!readfargs(&fa, fp)) {
					fprintf(stderr,
				"%s: bad argument syntax for %s \"%s\"\n",
							inp, typ, id);
					exit(1);
				}
				cvtprim(inp, mod, typ, id, &fa);
				freefargs(&fa);
			}
			break;
		}
	printf("# End conversion from: %s\n", inp);
	if (inp[0] == '!')
		pclose(fp);
	else
		fclose(fp);
#undef mod
#undef typ
#undef id
#undef alias
}


cvtprim(inp, mod, typ, id, fa)	/* process Radiance primitive */
char	*inp, *mod, *typ, *id;
FUNARGS	*fa;
{
	int	(*df)();

	df = (int (*)())lu_find(&rdispatch, typ)->data;
	if (df != NULL) {				/* convert */
		if ((*df)(mod, typ, id, fa) < 0) {
			fprintf(stderr, "%s: bad %s \"%s\"\n", typ, id);
			exit(1);
		}
	} else if (lu_find(&rmats, mod)->data != NULL)	/* make alias */
		newmat(id, mod);
}


newmat(id, alias)		/* add a modifier to the alias list */
char	*id;
char	*alias;
{
	register LUENT	*lp, *lpa;

	if (alias != NULL) {			/* look up alias */
		if ((lpa = lu_find(&rmats, alias)) == NULL)
			goto memerr;
		if (lpa->data == NULL)
			alias = NULL;		/* doesn't exist! */
	}
	if ((lp = lu_find(&rmats, id)) == NULL)	/* look up material */
		goto memerr;
	if (alias != NULL && lp->data == lpa->key)
		return;			/* alias set already */
	if (lp->data == NULL) {			/* allocate material */
		if ((lp->key = (char *)malloc(strlen(id)+1)) == NULL)
			goto memerr;
		strcpy(lp->key, id);
	}
	if (alias == NULL) {			/* set this material */
		lp->data = lp->key;
		printf("m %s =\n", id);
	} else {				/* set this alias */
		lp->data = lpa->key;
		printf("m %s = %s\n", id, alias);
	}
	strcpy(curmat, id);
	return;
memerr:
	fputs("Out of memory in newmat!\n", stderr);
	exit(1);
}


setmat(id)			/* set material to this one */
char	*id;
{
	if (!strcmp(id, curmat))	/* already set? */
		return;
	printf("m %s\n", id);
	strcpy(curmat, id);
}


init()			/* initialize dispatch table and output */
{
	lu_init(&rdispatch, 22);
	add2dispatch("polygon", o_face);
	add2dispatch("cone", o_cone);
	add2dispatch("cup", o_cone);
	add2dispatch("sphere", o_sphere);
	add2dispatch("bubble", o_sphere);
	add2dispatch("cylinder", o_cylinder);
	add2dispatch("tube", o_cylinder);
	add2dispatch("ring", o_ring);
	add2dispatch("instance", o_instance);
	add2dispatch("plastic", o_plastic);
	add2dispatch("plastic2", o_plastic);
	add2dispatch("metal", o_metal);
	add2dispatch("metal2", o_metal);
	add2dispatch("glass", o_glass);
	add2dispatch("trans", o_trans);
	add2dispatch("trans2", o_trans);
	add2dispatch("mirror", o_mirror);
	add2dispatch("light", o_light);
	add2dispatch("spotlight", o_light);
	add2dispatch("glow", o_light);
	add2dispatch("illum", o_illum);
	puts("# The following was converted from Radiance scene input");
	if (unit_mult < .999 || unit_mult > 1.001)
		printf("xf -s %.4e\n", unit_mult);
}


uninit()			/* mark end of MGF file */
{
	if (unit_mult < .999 || unit_mult > 1.001)
		puts("xf");
	puts("# End of data converted from Radiance scene input");
	lu_done(&rdispatch);
	lu_done(&rmats);
}


add2dispatch(name, func)	/* add function to dispatch table */
char	*name;
int	(*func)();
{
	register LUENT	*lp;

	lp = lu_find(&rdispatch, name);
	if (lp->key != NULL) {
		fputs(name, stderr);
		fputs(": duplicate dispatch entry!\n", stderr);
		exit(1);
	}
	lp->key = name;
	lp->data = (char *)func;
}


char	VKFMT[] = "%+1.9e %+1.9e %+1.9e";
#define VKLEN		64

#define mkvkey(k,v)	sprintf(k, VKFMT, (v)[0], (v)[1], (v)[2])

#define NVERTS		256

long	clock;		/* incremented at each vertex request */

struct vert {
	long	lused;		/* when last used (0 if unassigned) */
	FVECT	p;		/* track point position only */
} vert[NVERTS];

LUTAB	vertab = LU_SINIT(free,NULL);	/* our vertex lookup table */


char *
getvertid(vp)			/* get/set vertex ID for this point */
FVECT	vp;
{
	static char	vname[6];
	char	vkey[VKLEN];
	register LUENT	*lp;
	register int	i, vndx;

	if (!vertab.tsiz && !lu_init(&vertab, NVERTS))
		goto memerr;
	clock++;			/* increment counter */
	mkvkey(vkey, vp);
	if ((lp = lu_find(&vertab, vkey)) == NULL)
		goto memerr;
	if (lp->data == NULL) {		/* allocate new vertex entry */
		if (lp->key != NULL)		/* reclaim deleted entry */
			vertab.ndel--;
		else {
			if ((lp->key = (char *)malloc(VKLEN)) == NULL)
				goto memerr;
			strcpy(lp->key, vkey);
		}
		vndx = 0;			/* find oldest vertex */
		for (i = 1; i < NVERTS; i++)
			if (vert[i].lused < vert[vndx].lused)
				vndx = i;
		if (vert[vndx].lused) {		/* free old entry first */
			mkvkey(vkey, vert[vndx].p);
			lu_delete(&vertab, vkey);
		}
		vert[vndx].lused = clock;			/* assign it */
		VCOPY(vert[vndx].p, vp);
		printf("v v%d =\np %.15g %.15g %.15g\n",	/* print it */
				vndx, vp[0], vp[1], vp[2]);
		lp->data = (char *)&vert[vndx];			/* set it */
	} else
		vndx = (struct vert *)lp->data - vert;
	sprintf(vname, "v%d", vndx);
	return(vname);
memerr:
	fputs("Out of memory in getvertid!\n", stderr);
	exit(1);
}


int
o_face(mod, typ, id, fa)		/* print out a polygon */
char	*mod, *typ, *id;
FUNARGS	*fa;
{
	char	entbuf[512];
	register char	*cp1, *cp2;
	register int	i;

	if (fa->nfargs < 9 | fa->nfargs % 3)
		return(-1);
	setmat(mod);
	printf("o %s\n", id);
	cp1 = entbuf;
	*cp1++ = 'f';
	for (i = 0; i < fa->nfargs; i += 3) {
		cp2 = getvertid(fa->farg + i);
		*cp1++ = ' ';
		while ((*cp1 = *cp2++))
			cp1++;
	}
	puts(entbuf);
	puts("o");
	return(0);
}


int
o_cone(mod, typ, id, fa)	/* print out a cone */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	if (fa->nfargs != 8)
		return(-1);
	setmat(mod);
	printf("o %s\n", id);
	printf("v cv1 =\np %.12g %.12g %.12g\n",
			fa->farg[0], fa->farg[1], fa->farg[2]);
	printf("v cv2 =\np %.12g %.12g %.12g\n",
			fa->farg[3], fa->farg[4], fa->farg[5]);
	if (typ[1] == 'u')			/* cup -> inverted cone */
		printf("cone cv1 %.12g cv2 %.12g\n",
				-fa->farg[6], -fa->farg[7]);
	else
		printf("cone cv1 %.12g cv2 %.12g\n",
				fa->farg[6], fa->farg[7]);
	puts("o");
	return(0);
}


int
o_sphere(mod, typ, id, fa)	/* print out a sphere */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	if (fa->nfargs != 4)
		return(-1);
	setmat(mod);
	printf("o %s\n", id);
	printf("v cent =\np %.12g %.12g %.12g\n",
			fa->farg[0], fa->farg[1], fa->farg[2]);
	printf("sph cent %.12g\n", typ[0]=='b' ? -fa->farg[3] : fa->farg[3]);
	puts("o");
	return(0);
}


int
o_cylinder(mod, typ, id, fa)	/* print out a cylinder */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	if (fa->nfargs != 7)
		return(-1);
	setmat(mod);
	printf("o %s\n", id);
	printf("v cv1 =\np %.12g %.12g %.12g\n",
			fa->farg[0], fa->farg[1], fa->farg[2]);
	printf("v cv2 =\np %.12g %.12g %.12g\n",
			fa->farg[3], fa->farg[4], fa->farg[5]);
	printf("cyl cv1 %.12g cv2\n",
			typ[0]=='t' ? -fa->farg[6] : fa->farg[6]);
	puts("o");
	return(0);
}


int
o_ring(mod, typ, id, fa)	/* print out a ring */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	if (fa->nfargs != 8)
		return(-1);
	setmat(mod);
	printf("o %s\n", id);
	printf("v cent =\np %.12g %.12g %.12g\n",
			fa->farg[0], fa->farg[1], fa->farg[2]);
	printf("n %.12g %.12g %.12g\n",
			fa->farg[3], fa->farg[4], fa->farg[5]);
	if (fa->farg[6] < fa->farg[7])
		printf("ring cent %.12g %.12g\n",
				fa->farg[6], fa->farg[7]);
	else
		printf("ring cent %.12g %.12g\n",
				fa->farg[7], fa->farg[6]);
	puts("o");
	return(0);
}


int
o_instance(mod, typ, id, fa)	/* convert an instance */
char	*mod, *typ, *id;
FUNARGS	*fa;
{
	return(0);		/* this is too damned difficult! */
}


int
o_source(mod, typ, id, fa)	/* convert a source */
char	*mod, *typ, *id;
FUNARGS	*fa;
{
	return(0);		/* there is no MGF equivalent! */
}


int
o_illum(mod, typ, id, fa)	/* convert an illum material */
char	*mod, *typ, *id;
FUNARGS	*fa;
{
	if (fa->nsargs == 1 && strcmp(fa->sarg[0], VOIDID)) {
		newmat(id, fa->sarg[0]);	/* just create alias */
		return(0);
	}
					/* else create invisible material */
	newmat(id, NULL);
	puts("ts 1 0");
	return(0);
}


int
o_plastic(mod, typ, id, fa)	/* convert a plastic material */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	COLOR	cxyz, rrgb;
	double	d;

	if (fa->nfargs != (typ[7]=='2' ? 6 : 5))
		return(-1);
	newmat(id, NULL);
	rrgb[0] = fa->farg[0]; rrgb[1] = fa->farg[1]; rrgb[2] = fa->farg[2];
	rgb_cie(cxyz, rrgb);
	puts("c");				/* put diffuse component */
	d = cxyz[0] + cxyz[1] + cxyz[2];
	if (d > FTINY)
		printf("cxy %.4f %.4f\n", cxyz[0]/d, cxyz[1]/d);
	printf("rd %.4f\n", cxyz[1]*(1. - fa->farg[3]));
	puts("c");				/* put specular component */
	printf("rs %.4f %.4f\n", fa->farg[3],
			typ[7]=='2' ? .5*(fa->farg[4] + fa->farg[5]) :
					fa->farg[4]);
	return(0);
}


int
o_metal(mod, typ, id, fa)	/* convert a metal material */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	COLOR	cxyz, rrgb;
	double	d;

	if (fa->nfargs != (typ[5]=='2' ? 6 : 5))
		return(-1);
	newmat(id, NULL);
	rrgb[0] = fa->farg[0]; rrgb[1] = fa->farg[1]; rrgb[2] = fa->farg[2];
	rgb_cie(cxyz, rrgb);
	puts("c");				/* put diffuse component */
	d = cxyz[0] + cxyz[1] + cxyz[2];
	if (d > FTINY)
		printf("cxy %.4f %.4f\n", cxyz[0]/d, cxyz[1]/d);
	printf("rd %.4f\n", cxyz[1]*(1. - fa->farg[3]));
						/* put specular component */
	printf("rs %.4f %.4f\n", cxyz[1]*fa->farg[3],
			typ[5]=='2' ? .5*(fa->farg[4] + fa->farg[5]) :
					fa->farg[4]);
	return(0);
}


int
o_glass(mod, typ, id, fa)	/* convert a glass material */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	COLOR	cxyz, rrgb, trgb;
	double	nrfr = 1.52, F, d;
	register int	i;

	if (fa->nfargs != 3 && fa->nfargs != 4)
		return(-1);
	newmat(id, NULL);
	if (fa->nfargs == 4)
		nrfr = fa->farg[3];
	F = (1. - nrfr)/(1. + nrfr);		/* use normal incidence */
	F *= F;
	for (i = 0; i < 3; i++) {
		rrgb[i] = (1. - F)*(1. - F)/(1. - F*F*fa->farg[i]*fa->farg[i]);
		trgb[i] = F * (1. + (1. - 2.*F)*fa->farg[i]) /
				(1. - F*F*fa->farg[i]*fa->farg[i]);
	}
	rgb_cie(cxyz, rrgb);			/* put reflected component */
	puts("c");
	d = cxyz[0] + cxyz[1] + cxyz[2];
	if (d > FTINY)
		printf("cxy %.4f %.4f\n", cxyz[0]/d, cxyz[1]/d);
	printf("rs %.4f 0\n", cxyz[1]);
	rgb_cie(cxyz, trgb);			/* put transmitted component */
	puts("c");
	d = cxyz[0] + cxyz[1] + cxyz[2];
	if (d > FTINY)
		printf("cxy %.4f %.4f\n", cxyz[0]/d, cxyz[1]/d);
	printf("ts %.4f 0\n", cxyz[1]);
	return(0);
}


int
o_mirror(mod, typ, id, fa)	/* convert a mirror material */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	COLOR	cxyz, rrgb;
	double	d;

	if (fa->nsargs == 1) {			/* use alternate material */
		newmat(id, fa->sarg[0]);
		return(0);
	}
	if (fa->nfargs != 3)
		return(-1);
	newmat(id, NULL);
	rrgb[0] = fa->farg[0]; rrgb[1] = fa->farg[1]; rrgb[2] = fa->farg[2];
	rgb_cie(cxyz, rrgb);
	puts("c");				/* put specular component */
	d = cxyz[0] + cxyz[1] + cxyz[2];
	if (d > FTINY)
		printf("cxy %.4f %.4f\n", cxyz[0]/d, cxyz[1]/d);
	printf("rs %.4f 0\n", cxyz[1]);
	return(0);
}


int
o_trans(mod, typ, id, fa)	/* convert a trans material */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	COLOR	cxyz, rrgb;
	double	rough, trans, tspec, d;

	if (typ[4] == '2') {		/* trans2 */
		if (fa->nfargs != 8)
			return(-1);
		rough = .5*(fa->farg[4] + fa->farg[5]);
		trans = fa->farg[6];
		tspec = fa->farg[7];
	} else {			/* trans */
		if (fa->nfargs != 7)
			return(-1);
		rough = fa->farg[4];
		trans = fa->farg[5];
		tspec = fa->farg[6];
	}
	newmat(id, NULL);
	rrgb[0] = fa->farg[0]; rrgb[1] = fa->farg[1]; rrgb[2] = fa->farg[2];
	rgb_cie(cxyz, rrgb);
	puts("c");				/* put transmitted diffuse */
	d = cxyz[0] + cxyz[1] + cxyz[2];
	if (d > FTINY)
		printf("cxy %.4f %.4f\n", cxyz[0]/d, cxyz[1]/d);
	printf("td %.4f\n", cxyz[1]*trans*(1. - fa->farg[3])*(1. - tspec));
						/* put transmitted specular */
	printf("ts %.4f %.4f\n", cxyz[1]*trans*tspec*(1. - fa->farg[3]), rough);
						/* put reflected diffuse */
	printf("rd %.4f\n", cxyz[1]*(1. - fa->farg[3])*(1. - trans));
	puts("c");				/* put reflected specular */
	printf("rs %.4f %.4f\n", fa->farg[3], rough);
	return(0);
}


int
o_light(mod, typ, id, fa)		/* convert a light type */
char	*mod, *typ, *id;
register FUNARGS	*fa;
{
	COLOR	cxyz, rrgb;
	double	d;

	if (fa->nfargs < 3)
		return(-1);
	newmat(id, NULL);
	rrgb[0] = fa->farg[0]; rrgb[1] = fa->farg[1]; rrgb[2] = fa->farg[2];
	rgb_cie(cxyz, rrgb);
	d = cxyz[0] + cxyz[1] + cxyz[2];
	puts("c");
	if (d > FTINY)
		printf("cxy %.4f %.4f\n", cxyz[0]/d, cxyz[1]/d);
	printf("ed %.4g\n", cxyz[1]);
	return(0);
}
