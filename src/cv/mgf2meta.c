/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert MGF (Materials and Geometry Format) to Metafile 2-d graphics
 */

#include <stdio.h>
#include <math.h>
#include "mgflib/parser.h"

#define	MX(v)	(int)(((1<<14)-1)*(v)[(proj_axis+1)%3])
#define	MY(v)	(int)(((1<<14)-1)*(v)[(proj_axis+2)%3])

int	r_face();
int	proj_axis;
double	limit[3][2];
int	layer;

extern int	mg_nqcdivs;


main(argc, argv)		/* convert files to stdout */
int	argc;
char	*argv[];
{
	int	i;
				/* initialize dispatch table */
	mg_ehand[MG_E_FACE] = r_face;
	mg_ehand[MG_E_POINT] = c_hvertex;
	mg_ehand[MG_E_VERTEX] = c_hvertex;
	mg_ehand[MG_E_XF] = xf_handler;
	mg_nqcdivs = 3;		/* reduce object subdivision */
	mg_init();		/* initialize the parser */
					/* get arguments */
	if (argc < 8 || (proj_axis = argv[1][0]-'x') < 0 || proj_axis > 2)
		goto userr;
	limit[0][0] = atof(argv[2]); limit[0][1] = atof(argv[3]);
	limit[1][0] = atof(argv[4]); limit[1][1] = atof(argv[5]);
	limit[2][0] = atof(argv[6]); limit[2][1] = atof(argv[7]);
	
	if (argc == 8) {		/* convert stdin */
		if (mg_load(NULL) != MG_OK)
			exit(1);
	} else				/* convert each file */
		for (i = 8; i < argc; i++) {
			if (mg_load(argv[i]) != MG_OK)
				exit(1);
			if (++layer >= 16) {
				mendpage();
				layer = 0;
			}
		}
	mendpage();			/* print page */
	mdone();			/* close output */
	exit(0);
userr:
	fprintf(stderr, "Usage: %s {x|y|z} xmin xmax ymin ymax zmin zmax [file.mgf] ..\n",
			argv[0]);
	exit(1);
}


int
r_face(ac, av)			/* convert a face */
int	ac;
char	**av;
{
	static FVECT	bbmin = {0,0,0}, bbmax = {1,1,1};
	register int	i, j;
	register C_VERTEX	*cv;
	FVECT	v1, v2, vo;
	int	newline = 1;

	if (ac < 4)
		return(MG_EARGC);
						/* connect to last point */
	if ((cv = c_getvert(av[ac-1])) == NULL)
		return(MG_EUNDEF);
	xf_xfmpoint(vo, cv->p);
	for (j = 0; j < 3; j++)
		vo[j] = (vo[j] - limit[j][0])/(limit[j][1]-limit[j][0]);
	for (i = 1; i < ac; i++) {		/* go around face */
		if ((cv = c_getvert(av[i])) == NULL)
			return(MG_EUNDEF);
		xf_xfmpoint(v2, cv->p);
		for (j = 0; j < 3; j++)
			v2[j] = (v2[j] - limit[j][0])/(limit[j][1]-limit[j][0]);
		VCOPY(v1, vo);
		VCOPY(vo, v2);
		if (clip(v1, v2, bbmin, bbmax)) {
			mline(MX(v1), MY(v1), layer/4, 0, layer%4);
			mdraw(MX(v2), MY(v2));
		}
	}
	return(MG_OK);
}
