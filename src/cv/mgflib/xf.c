/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Routines for 4x4 homogeneous, rigid-body transformations
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "parser.h"

#define  d2r(a)		((PI/180.)*(a))

#define  checkarg(a,l)	if (av[i][a] || badarg(ac-i-1,av+i+1,l)) goto done

MAT4  m4ident = MAT4IDENT;

static MAT4  m4tmp;		/* for efficiency */

int	xf_argc;			/* total # transform args. */
char	**xf_argv;			/* transform arguments */
XF_SPEC	*xf_context;			/* current context */

static int	xf_maxarg;		/* # allocated arguments */


int
xf_handler(ac, av)		/* handle xf entity */
int	ac;
char	**av;
{
	register int	i;
	register XF_SPEC	*spec;
	XF	thisxf;

	if (ac == 1) {			/* pop top transform */
		if ((spec = xf_context) == NULL)
			return(MG_OK);		/* should be error? */
		while (xf_argc > spec->xav0)
			free((MEM_PTR)xf_argv[--xf_argc]);
		xf_argv[xf_argc] = NULL;
		xf_context = spec->prev;
		free((MEM_PTR)spec);
		return(MG_OK);
	}
					/* translate new specification */
	if (xf(&thisxf, ac-1, av+1) != ac-1)
		return(MG_ETYPE);
					/* allocate space for new transform */
	spec = (XF_SPEC *)malloc(sizeof(XF_SPEC));
	if (spec == NULL)
		return(MG_EMEM);
	spec->xav0 = xf_argc;
	spec->xac = ac-1;
					/* and store new xf arguments */
	if (xf_argc+ac > xf_maxarg) {
		if (!xf_maxarg)
			xf_argv = (char **)malloc(
					(xf_maxarg=ac)*sizeof(char *));
		else
			xf_argv = (char **)realloc((MEM_PTR)xf_argv,
					(xf_maxarg+=ac)*sizeof(char *));
		if (xf_argv == NULL)
			return(MG_EMEM);
	}
	for (i = 0; i < ac-1; i++) {
		xf_argv[xf_argc] = (char *)malloc(strlen(av[i+1])+1);
		if (xf_argv[xf_argc] == NULL)
			return(MG_EMEM);
		strcpy(xf_argv[xf_argc++], av[i+1]);
	}
	xf_argv[xf_argc] = NULL;
					/* compute total transformation */
	if (xf_context != NULL) {
		multmat4(spec->xf.xfm, xf_context->xf.xfm, thisxf.xfm);
		spec->xf.sca = xf_context->xf.sca * thisxf.sca;
	} else
		spec->xf = thisxf;
	spec->prev = xf_context;	/* push new transform onto stack */
	xf_context = spec;
	return(MG_OK);
}


void
xf_xfmpoint(v1, v2)		/* transform a point by the current matrix */
FVECT	v1, v2;
{
	if (xf_context == NULL) {
		v1[0] = v2[0];
		v1[1] = v2[1];
		v1[2] = v2[2];
		return;
	}
	multp3(v1, v2, xf_context->xf.xfm);
}


void
xf_xfmvect(v1, v2)		/* transform a vector using current matrix */
FVECT	v1, v2;
{
	if (xf_context == NULL) {
		v1[0] = v2[0];
		v1[1] = v2[1];
		v1[2] = v2[2];
		return;
	}
	multv3(v1, v2, xf_context->xf.xfm);
}


void
xf_rotvect(v1, v2)		/* rotate a vector using current matrix */
FVECT	v1, v2;
{
	xf_xfmvect(v1, v2);
	if (xf_context == NULL)
		return;
	v1[0] /= xf_context->xf.sca;
	v1[1] /= xf_context->xf.sca;
	v1[2] /= xf_context->xf.sca;
}


double
xf_scale(d)			/* scale a number by the current transform */
double	d;
{
	if (xf_context == NULL)
		return(d);
	return(d*xf_context->xf.sca);
}


void
multmat4(m4a, m4b, m4c)		/* multiply m4b X m4c and put into m4a */
MAT4  m4a;
register MAT4  m4b, m4c;
{
	register int  i, j;
	
	for (i = 4; i--; )
		for (j = 4; j--; )
			m4tmp[i][j] = m4b[i][0]*m4c[0][j] +
				      m4b[i][1]*m4c[1][j] +
				      m4b[i][2]*m4c[2][j] +
				      m4b[i][3]*m4c[3][j];
	
	copymat4(m4a, m4tmp);
}


void
multv3(v3a, v3b, m4)	/* transform vector v3b by m4 and put into v3a */
FVECT  v3a;
register FVECT  v3b;
register MAT4  m4;
{
	m4tmp[0][0] = v3b[0]*m4[0][0] + v3b[1]*m4[1][0] + v3b[2]*m4[2][0];
	m4tmp[0][1] = v3b[0]*m4[0][1] + v3b[1]*m4[1][1] + v3b[2]*m4[2][1];
	m4tmp[0][2] = v3b[0]*m4[0][2] + v3b[1]*m4[1][2] + v3b[2]*m4[2][2];
	
	v3a[0] = m4tmp[0][0];
	v3a[1] = m4tmp[0][1];
	v3a[2] = m4tmp[0][2];
}


void
multp3(p3a, p3b, m4)		/* transform p3b by m4 and put into p3a */
register FVECT  p3a;
FVECT  p3b;
register MAT4  m4;
{
	multv3(p3a, p3b, m4);	/* transform as vector */
	p3a[0] += m4[3][0];	/* translate */
	p3a[1] += m4[3][1];
	p3a[2] += m4[3][2];
}


int
xf(ret, ac, av)			/* get transform specification */
register XF  *ret;
int  ac;
char  *av[];
{
	MAT4  xfmat, m4;
	double  xfsca, dtmp;
	int  i, icnt;

	setident4(ret->xfm);
	ret->sca = 1.0;

	icnt = 1;
	setident4(xfmat);
	xfsca = 1.0;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			checkarg(2,"fff");
			m4[3][0] = atof(av[++i]);
			m4[3][1] = atof(av[++i]);
			m4[3][2] = atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,"f");
				dtmp = d2r(atof(av[++i]));
				m4[1][1] = m4[2][2] = cos(dtmp);
				m4[2][1] = -(m4[1][2] = sin(dtmp));
				break;
			case 'y':
				checkarg(3,"f");
				dtmp = d2r(atof(av[++i]));
				m4[0][0] = m4[2][2] = cos(dtmp);
				m4[0][2] = -(m4[2][0] = sin(dtmp));
				break;
			case 'z':
				checkarg(3,"f");
				dtmp = d2r(atof(av[++i]));
				m4[0][0] = m4[1][1] = cos(dtmp);
				m4[1][0] = -(m4[0][1] = sin(dtmp));
				break;
			default:
				goto done;
			}
			break;

		case 's':			/* scale */
			checkarg(2,"f");
			dtmp = atof(av[i+1]);
			if (dtmp == 0.0) goto done;
			i++;
			xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = dtmp;
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,"");
				xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				checkarg(3,"");
				xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				checkarg(3,"");
				xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				goto done;
			}
			break;

		case 'i':			/* iterate */
			checkarg(2,"i");
			while (icnt-- > 0) {
				multmat4(ret->xfm, ret->xfm, xfmat);
				ret->sca *= xfsca;
			}
			icnt = atoi(av[++i]);
			setident4(xfmat);
			xfsca = 1.0;
			continue;

		default:
			goto done;

		}
		multmat4(xfmat, xfmat, m4);
	}
done:
	while (icnt-- > 0) {
		multmat4(ret->xfm, ret->xfm, xfmat);
		ret->sca *= xfsca;
	}
	return(i);
}
