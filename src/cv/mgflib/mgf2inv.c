/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert MGF to Inventor file.
 *
 *	December 1995	Greg Ward
 */

#include <stdio.h>

#include <math.h>

#include "parser.h"

#include "lookup.h"

#ifndef PI
#define PI		3.14159265358979323846
#endif

#define	TABSTOP		8	/* assumed number of characters per tab */
#define	SHIFTW		2	/* nesting shift width */
#define MAXIND		15	/* maximum indent level */

extern int	i_comment(), i_object(), i_xf(), i_include(),
		i_cyl(), i_face(), i_sph();

int	vrmlout = 0;		/* VRML output desired? */

int	instancing = 0;		/* are we in an instance? */

char	tabs[MAXIND*SHIFTW+1];	/* current tab-in string */


main(argc, argv)
int	argc;
char	*argv[];
{
	int	i;
				/* initialize dispatch table */
	mg_ehand[MG_E_COMMENT] = i_comment;	/* we pass comments */
	mg_ehand[MG_E_COLOR] = c_hcolor;	/* they get color */
	mg_ehand[MG_E_CMIX] = c_hcolor;		/* they mix colors */
	mg_ehand[MG_E_CSPEC] = c_hcolor;	/* they get spectra */
	mg_ehand[MG_E_CXY] = c_hcolor;		/* they get chromaticities */
	mg_ehand[MG_E_CCT] = c_hcolor;		/* they get color temp's */
	mg_ehand[MG_E_CYL] = i_cyl;		/* we do cylinders */
	mg_ehand[MG_E_ED] = c_hmaterial;	/* they get emission */
	mg_ehand[MG_E_FACE] = i_face;		/* we do faces */
	mg_ehand[MG_E_MATERIAL] = c_hmaterial;	/* they get materials */
	mg_ehand[MG_E_NORMAL] = c_hvertex;	/* they get normals */
	mg_ehand[MG_E_OBJECT] = i_object;	/* we track object names */
	mg_ehand[MG_E_POINT] = c_hvertex;	/* they get points */
	mg_ehand[MG_E_RD] = c_hmaterial;	/* they get diffuse refl. */
	mg_ehand[MG_E_RS] = c_hmaterial;	/* they get specular refl. */
	mg_ehand[MG_E_SIDES] = c_hmaterial;	/* they get # sides */
	mg_ehand[MG_E_SPH] = i_sph;		/* we do spheres */
	mg_ehand[MG_E_TD] = c_hmaterial;	/* they get diffuse trans. */
	mg_ehand[MG_E_TS] = c_hmaterial;	/* they get specular trans. */
	mg_ehand[MG_E_VERTEX] = c_hvertex;	/* they get vertices */
	mg_ehand[MG_E_XF] = i_xf;		/* we track transforms */
	mg_ehand[MG_E_INCLUDE] = i_include;	/* we include files */
	mg_init();		/* initialize the parser */
	i = 1;			/* get options and print format line */
	if (i < argc && !strncmp(argv[i], "-vrml", strlen(argv[i]))) {
		printf("#VRML 1.0 ascii\n");
		vrmlout++;
		i++;
	} else
		printf("#Inventor V2.0 ascii\n");
	printf("## Translated from MGF Version %d.%d\n", MG_VMAJOR, MG_VMINOR);
	printf("Separator {\n");		/* begin root node */
						/* general properties */
	printf("MaterialBinding { value OVERALL }\n");
	printf("NormalBinding { value PER_VERTEX }\n");
	printf("ShapeHints {\n");
	printf("\tvertexOrdering CLOCKWISE\n");
	printf("\tfaceType UNKNOWN_FACE_TYPE\n");
	printf("}\n");
	if (i == argc) {	/* load standard input */
		if (mg_load(NULL) != MG_OK)
			exit(1);
		if (mg_nunknown)
			printf("## %s: %u unknown entities\n",
					argv[0], mg_nunknown);
	}
				/* load MGF files */
	for ( ; i < argc; i++) {
		printf("## %s %s ##############################\n",
				argv[0], argv[i]);
		mg_nunknown = 0;
		if (mg_load(argv[i]) != MG_OK)
			exit(1);
		if (mg_nunknown)
			printf("## %s %s: %u unknown entities\n",
					argv[0], argv[i], mg_nunknown);
	}
	printf("}\n");				/* close root node */
	exit(0);
}


indent(deeper)				/* indent in or out */
int	deeper;
{
	static int	level;		/* current nesting level */
	register int	i;
	register char	*cp;

	if (deeper) level++;		/* in or out? */
	else if (level > 0) level--;
					/* compute actual shift */
	if ((i = level) > MAXIND) i = MAXIND;
	cp = tabs;
	for (i *= SHIFTW; i >= TABSTOP; i -= TABSTOP)
		*cp++ = '\t';
	while (i--)
		*cp++ = ' ';
	*cp = '\0';
}


int
i_comment(ac, av)			/* transfer comment as is */
int	ac;
char	**av;
{
	if (instancing)
		return(MG_OK);
	fputs(tabs, stdout);
	putchar('#');			/* Inventor comment character */
	while (--ac > 0) {
		putchar(' ');
		fputs(*++av, stdout);
	}
	putchar('\n');
	return(MG_OK);
}


int
i_object(ac, av)			/* group object name */
int	ac;
char	**av;
{
	static int	objnest;

	if (instancing)
		return(MG_OK);
	if (ac == 2) {				/* start group */
		printf("%sDEF %s Group {\n", tabs, av[1]);
		indent(1);
		objnest++;
		return(MG_OK);
	}
	if (ac == 1) {				/* end group */
		if (--objnest < 0)
			return(MG_ECNTXT);
		indent(0);
		fputs(tabs, stdout);
		fputs("}\n", stdout);
		return(MG_OK);
	}
	return(MG_EARGC);
}


int
i_xf(ac, av)				/* transform object(s) */
int	ac;
char	**av;
{
	static long	xfid;
	register XF_SPEC	*spec;

	if (instancing)
		return(MG_OK);
	if (ac == 1) {			/* end of transform */
		if ((spec = xf_context) == NULL)
			return(MG_ECNTXT);
		indent(0);			/* close original segment */
		printf("%s}\n", tabs);
		indent(0);
		printf("%s}\n", tabs);
		if (spec->xarr != NULL) {	/* check for iteration */
			register struct xf_array	*ap = spec->xarr;
			register int	n;

			ap->aarg[ap->ndim-1].i = 1;	/* iterate array */
			for ( ; ; ) {
				n = ap->ndim-1;
				while (ap->aarg[n].i < ap->aarg[n].n) {
					sprintf(ap->aarg[n].arg, "%d",
							ap->aarg[n].i);
					printf("%sSeparator {\n", tabs);
					indent(1);
					(void)put_xform(spec);
					printf("%sUSE _xf%ld\n", tabs,
							spec->xid);
					indent(0);
					printf("%s}\n", tabs);
					++ap->aarg[n].i;
				}
				ap->aarg[n].i = 0;
				(void)strcpy(ap->aarg[n].arg, "0");
				while (n-- && ++ap->aarg[n].i >= ap->aarg[n].n) {
					ap->aarg[n].i = 0;
					(void)strcpy(ap->aarg[n].arg, "0");
				}
				if (n < 0)
					break;
				sprintf(ap->aarg[n].arg, "%d", ap->aarg[n].i);
			}
		}
						/* pop transform */
		xf_context = spec->prev;
		free_xf(spec);
		return(MG_OK);
	}
					/* else allocate new transform */
	if ((spec = new_xf(ac-1, av+1)) == NULL)
		return(MG_EMEM);
	spec->xid = ++xfid;		/* assign unique ID */
	spec->prev = xf_context;	/* push onto stack */
	xf_context = spec;
					/* translate xf specification */
	printf("%sSeparator {\n", tabs);
	indent(1);
	if (put_xform(spec) < 0)
		return(MG_ETYPE);
	printf("%sDEF _xf%ld Group {\n", tabs, spec->xid);	/* begin */
	indent(1);
	return(MG_OK);
}


int
put_xform(spec)			/* translate and print transform */
register XF_SPEC	*spec;
{
	register char	**av;
	register int	n;

	n = xf_ac(spec) - xf_ac(spec->prev);
	if (xf(&spec->xf, n, av=xf_av(spec)) != n)
		return(-1);
	printf("%sMatrixTransform {\n", tabs);
	indent(1);
	printf("%s# xf", tabs);		/* put out original as comment */
	while (n--) {
		putchar(' ');
		fputs(*av++, stdout);
	}
	putchar('\n');			/* put out computed matrix */
	printf("%smatrix %13.9g %13.9g %13.9g %13.9g\n", tabs,
			spec->xf.xfm[0][0], spec->xf.xfm[0][1],
			spec->xf.xfm[0][2], spec->xf.xfm[0][3]);
	for (n = 1; n < 4; n++)
		printf("%s       %13.9g %13.9g %13.9g %13.9g\n", tabs,
				spec->xf.xfm[n][0], spec->xf.xfm[n][1],
				spec->xf.xfm[n][2], spec->xf.xfm[n][3]);
	indent(0);
	printf("%s}\n", tabs);
	return(0);
}


int
i_include(ac, av)		/* include an MGF file */
int	ac;
char	**av;
{
	static LUTAB	in_tab = LU_SINIT(free,free);	/* instance table */
	static long	nincl;
	LUENT	*lp;
	char	*xfarg[MG_MAXARGC];
	MG_FCTXT	ictx;
	register int	i;

	if (ac < 2)
		return(MG_EARGC);
	if (ac > 2) {				/* start transform if one */
		xfarg[0] = mg_ename[MG_E_XF];
		for (i = 1; i < ac-1; i++)
			xfarg[i] = av[i+1];
		xfarg[ac-1] = NULL;
		if ((i = mg_handle(MG_E_XF, ac-1, xfarg)) != MG_OK)
			return(i);
	}
						/* open include file */
	if ((i = mg_open(&ictx, av[1])) != MG_OK)
		return(i);
						/* check for instance */
	lp = lu_find(&in_tab, ictx.fname);
	if (lp == NULL) goto memerr;
	if (lp->data != NULL) {			/* reference original */
		instancing++;
		printf("%sUSE %s\n", tabs, lp->data);
		lp = NULL;
	} else {				/* else new original */
		lp->key = (char *)malloc(strlen(ictx.fname)+1);
		if (lp->key == NULL) goto memerr;
		(void)strcpy(lp->key, ictx.fname);
		if (ac > 2) {			/* use transform group */
			lp->data = (char *)malloc(16);
			if (lp->data == NULL) goto memerr;
			sprintf(lp->data, "_xf%ld", xf_context->xid);
		} else {			/* else make our own group */
			lp->data = (char *)malloc(strlen(av[1])+16);
			if (lp->data == NULL) goto memerr;
			sprintf(lp->data, "_%s%ld", av[1], ++nincl);
			printf("%sDEF %s Group {\n", tabs, lp->data);
			indent(1);
		}
	}
	while ((i = mg_read()) > 0) {		/* load include file */
		if (i >= MG_MAXLINE-1) {
			fprintf(stderr, "%s: %d: %s\n", ictx.fname,
					ictx.lineno, mg_err[MG_ELINE]);
			mg_close();
			return(MG_EINCL);
		}
		if ((i = mg_parse()) != MG_OK) {
			fprintf(stderr, "%s: %d: %s:\n%s", ictx.fname,
					ictx.lineno, mg_err[i],
					ictx.inpline);
			mg_close();
			return(MG_EINCL);
		}
	}
	if (lp == NULL)				/* end instance? */
		instancing--;
	else if (ac <= 2) {			/* else end group? */
		indent(0);
		printf("%s}\n", tabs);
	}
	mg_close();				/* close and end transform */
	if (ac > 2 && (i = mg_handle(MG_E_XF, 1, xfarg)) != MG_OK)
		return(i);
	return(MG_OK);
memerr:
	mg_close();
	return(MG_EMEM);
}


int
put_material()			/* put out current material */
{
	char	*mname = "mat";
	float	rgbval[3];

	if (c_cmname != NULL)
		mname = c_cmname;
	if (!c_cmaterial->clock) {	/* current, just use it */
		printf("%sUSE %s\n", tabs, mname);
		return(0);
	}
				/* else update definition */
	printf("%sDEF %s Group {\n", tabs, mname);
	indent(1);
	printf("%sMaterial {\n", tabs);
	indent(1);
	mgf2rgb(&c_cmaterial->rd_c, c_cmaterial->rd, rgbval);
	printf("%sambientcolor %.4f %.4f %.4f\n", tabs,
			rgbval[0], rgbval[1], rgbval[2]);
	printf("%sdiffusecolor %.4f %.4f %.4f\n", tabs,
			rgbval[0], rgbval[1], rgbval[2]);
	if (c_cmaterial->rs > FTINY) {
		mgf2rgb(&c_cmaterial->rs_c, c_cmaterial->rs, rgbval);
		printf("%sspecularcolor %.4f %.4f %.4f\n", tabs,
				rgbval[0], rgbval[1], rgbval[2]);
		printf("%sshininess %.3f\n", tabs, 1.-c_cmaterial->rs_a);
	}
	if (c_cmaterial->ed > FTINY) {
		mgf2rgb(&c_cmaterial->ed_c, 1.0, rgbval);
		printf("%semissiveColor %.4f %.4f %.4f\n", tabs,
				rgbval[0], rgbval[1], rgbval[2]);
	}
	if (c_cmaterial->ts > FTINY)
		printf("%stransparency %.4f\n", tabs,
				c_cmaterial->ts + c_cmaterial->td);
	indent(0);
	printf("%s}\n", tabs);
	printf("%sShapeHints { shapeType %s }\n", tabs,
			c_cmaterial->sided ? "SOLID" : "UNKNOWN_SHAPE_TYPE");
	indent(0);
	printf("%s}\n", tabs);
	c_cmaterial->clock = 0;
	return(0);
}


int
i_face(ac, av)			/* translate an N-sided face */
int	ac;
char	**av;
{
	C_VERTEX	*vl[MG_MAXARGC-1];
	int	donorms = 1;
	register int	i;

	if (instancing)
		return(MG_OK);
	if (ac < 4)
		return(MG_EARGC);
	printf("%sSeparator {\n", tabs);
	indent(1);
				/* put out current material */
	if (put_material() < 0)
		return(MG_EBADMAT);
				/* get vertex references */
	for (i = 0; i < ac-1; i++) {
		if ((vl[i] = c_getvert(av[i+1])) == NULL)
			return(MG_EUNDEF);
		if (is0vect(vl[i]->n))
			donorms = 0;
	}
				/* put out vertex coordinates */
	printf("%sCoordinate3 {\n", tabs);
	indent(1);
	printf("%spoint [ %13.9g %13.9g %13.9g", tabs,
			vl[0]->p[0], vl[0]->p[1], vl[0]->p[2]);
	for (i = 1; i < ac-1; i++)
		printf(",\n%s        %13.9g %13.9g %13.9g", tabs,
			vl[i]->p[0], vl[i]->p[1], vl[i]->p[2]);
	indent(0);
	printf(" ]\n%s}\n", tabs);
				/* put out normal coordinates */
	if (donorms) {
		printf("%sNormal {\n", tabs);
		indent(1);
		printf("%svector [ %13.9g %13.9g %13.9g", tabs,
			vl[0]->n[0], vl[0]->p[1], vl[0]->p[2]);
		for (i = 1; i < ac-1; i++)
			printf(",\n%s         %13.9g %13.9g %13.9g", tabs,
					vl[i]->n[0], vl[i]->n[1], vl[i]->n[2]);
		indent(0);
		printf(" ]\n%s}\n", tabs);
	} else
		printf("%sNormal { }\n", tabs);
				/* put out actual face */
	printf("%sIndexedFaceSet {\n", tabs);
	indent(1);
	printf("%scoordIndex [", tabs);
	for (i = 0; i < ac-1; i++)
		printf(" %d", i);
	printf(" ]\n");
	indent(0);
	printf("%s}\n", tabs);
	indent(0);
	printf("%s}\n", tabs);
	return(MG_OK);
}


int
i_sph(ac, av)			/* translate sphere description */
int	ac;
char	**av;
{
	register C_VERTEX	*cent;

	if (instancing)
		return(MG_OK);
	if (ac != 3)
		return(MG_EARGC);
	printf("%sSeparator {\n", tabs);
	indent(1);
				/* put out current material */
	if (put_material() < 0)
		return(MG_EBADMAT);
				/* get center */
	if ((cent = c_getvert(av[1])) == NULL)
		return(MG_EUNDEF);
				/* get radius */
	if (!isflt(av[2]))
		return(MG_ETYPE);
	printf("%sTranslation { translation %13.9g %13.9g %13.9g }\n", tabs,
			cent->p[0], cent->p[1], cent->p[2]);
	printf("%sSphere { radius %s }\n", tabs, av[2]);
	indent(0);
	printf("%s}\n", tabs);
	return(MG_OK);
}


int
i_cyl(ac, av)			/* translate a cylinder description */
int	ac;
char	**av;
{
	register C_VERTEX	*v1, *v2;
	FVECT	va;
	double	length, angle;

	if (instancing)
		return(MG_OK);
	if (ac != 4)
		return(MG_EARGC);
	printf("%sSeparator {\n", tabs);
	indent(1);
				/* put out current material */
	if (put_material() < 0)
		return(MG_EBADMAT);
				/* get endpoints */
	if ((v1 = c_getvert(av[1])) == NULL | (v2 = c_getvert(av[3])) == NULL)
		return(MG_EUNDEF);
				/* get radius */
	if (!isflt(av[2]))
		return(MG_ETYPE);
				/* compute transform */
	va[0] = v2->p[0] - v1->p[0];
	va[1] = v2->p[1] - v1->p[1];
	va[2] = v2->p[2] - v1->p[2];
	length = sqrt(DOT(va,va));
	angle = 180./PI * acos(va[1]/length);
	printf("%sRotation { rotation %.9g %.9g %.9g %.9g }\n", tabs,
			va[2], 0., -va[0], angle);
	printf("%sTranslation { translation %13.9g %13.9g %13.9g }\n", tabs,
			.5*(v1->p[0]+v2->p[0]), .5*(v1->p[1]+v2->p[1]),
			.5*(v1->p[2]+v2->p[2]));
				/* open-ended */
	printf("%sCylinder { parts SIDES height %13.9g radius %s }\n", tabs,
			length, av[2]);
	indent(0);
	printf("%s}\n", tabs);
	return(MG_OK);
}
