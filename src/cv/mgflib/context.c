/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Context handlers
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "lookup.h"

				/* default context values */
static C_COLOR		c_dfcolor = C_DEFCOLOR;
static C_MATERIAL	c_dfmaterial = C_DEFMATERIAL;
static C_VERTEX		c_dfvertex = C_DEFVERTEX;

				/* the unnamed contexts */
static C_COLOR		c_uncolor = C_DEFCOLOR;
static C_MATERIAL	c_unmaterial = C_DEFMATERIAL;
static C_VERTEX		c_unvertex = C_DEFVERTEX;

				/* the current contexts */
C_COLOR		*c_ccolor = &c_uncolor;
C_MATERIAL	*c_cmaterial = &c_unmaterial;
C_VERTEX	*c_cvertex = &c_unvertex;

static LUTAB	clr_tab = LU_SINIT(free,free);	/* color lookup table */
static LUTAB	mat_tab = LU_SINIT(free,free);	/* material lookup table */
static LUTAB	vtx_tab = LU_SINIT(free,free);	/* vertex lookup table */


int
c_hcolor(ac, av)		/* handle color entity */
int	ac;
register char	**av;
{
	register LUENT	*lp;

	switch (mg_entity(av[0])) {
	case MG_E_COLOR:	/* get/set color context */
		if (ac == 1) {		/* set unnamed color context */
			c_uncolor = c_dfcolor;
			c_ccolor = &c_uncolor;
			return(MG_OK);
		}
		lp = lu_find(&clr_tab, av[1]);	/* lookup context */
		if (lp == NULL)
			return(MG_EMEM);
		if (ac == 2) {		/* reestablish previous context */
			if (lp->data == NULL)
				return(MG_EUNDEF);
			c_ccolor = (C_COLOR *)lp->data;
			return(MG_OK);
		}
		if (av[2][0] != '=' || av[2][1])
			return(MG_ETYPE);
		if (lp->key == NULL) {	/* create new color context */
			lp->key = (char *)malloc(strlen(av[1])+1);
			if (lp->key == NULL)
				return(MG_EMEM);
			strcpy(lp->key, av[1]);
			lp->data = (char *)malloc(sizeof(C_COLOR));
			if (lp->data == NULL)
				return(MG_EMEM);
		}
		c_ccolor = (C_COLOR *)lp->data;
		if (ac == 3) {		/* use default template */
			*c_ccolor = c_dfcolor;
			return(MG_OK);
		}
		lp = lu_find(&clr_tab, av[3]);	/* lookup template */
		if (lp == NULL)
			return(MG_EMEM);
		if (lp->data == NULL)
			return(MG_EUNDEF);
		*c_ccolor = *(C_COLOR *)lp->data;
		if (ac > 4)
			return(MG_EARGC);
		return(MG_OK);
	case MG_E_CXY:		/* assign CIE XY value */
		if (ac != 3)
			return(MG_EARGC);
		if (!isflt(av[1]) || !isflt(av[2]))
			return(MG_ETYPE);
		c_ccolor->cx = atof(av[1]);
		c_ccolor->cy = atof(av[2]);
		if (c_ccolor->cx < 0. | c_ccolor->cy < 0. |
				c_ccolor->cx + c_ccolor->cy > 1.)
			return(MG_EILL);
		return(MG_OK);
	}
	return(MG_EUNK);
}


int
c_hmaterial(ac, av)		/* handle material entity */
int	ac;
register char	**av;
{
	register LUENT	*lp;

	switch (mg_entity(av[0])) {
	case MG_E_MATERIAL:	/* get/set material context */
		if (ac == 1) {		/* set unnamed material context */
			c_unmaterial = c_dfmaterial;
			c_cmaterial = &c_unmaterial;
			return(MG_OK);
		}
		lp = lu_find(&mat_tab, av[1]);	/* lookup context */
		if (lp == NULL)
			return(MG_EMEM);
		if (ac == 2) {		/* reestablish previous context */
			if (lp->data == NULL)
				return(MG_EUNDEF);
			c_cmaterial = (C_MATERIAL *)lp->data;
			return(MG_OK);
		}
		if (av[2][0] != '=' || av[2][1])
			return(MG_ETYPE);
		if (lp->key == NULL) {	/* create new material context */
			lp->key = (char *)malloc(strlen(av[1])+1);
			if (lp->key == NULL)
				return(MG_EMEM);
			strcpy(lp->key, av[1]);
			lp->data = (char *)malloc(sizeof(C_MATERIAL));
			if (lp->data == NULL)
				return(MG_EMEM);
		}
		c_cmaterial = (C_MATERIAL *)lp->data;
		if (ac == 3) {		/* use default template */
			*c_cmaterial = c_dfmaterial;
			c_cmaterial->name = lp->key;
			return(MG_OK);
		}
		lp = lu_find(&mat_tab, av[3]);	/* lookup template */
		if (lp == NULL)
			return(MG_EMEM);
		if (lp->data == NULL)
			return(MG_EUNDEF);
		*c_cmaterial = *(C_MATERIAL *)lp->data;
		c_cmaterial->name = lp->key;
		c_cmaterial->clock = 1;
		if (ac > 4)
			return(MG_EARGC);
		return(MG_OK);
	case MG_E_RD:		/* set diffuse reflectance */
		if (ac != 2)
			return(MG_EARGC);
		if (!isflt(av[1]))
			return(MG_ETYPE);
		c_cmaterial->rd = atof(av[1]);
		if (c_cmaterial->rd < 0. | c_cmaterial->rd > 1.)
			return(MG_EILL);
		c_cmaterial->rd_c = *c_ccolor;
		c_cmaterial->clock++;
		return(MG_OK);
	case MG_E_ED:		/* set diffuse emittance */
		if (ac != 2)
			return(MG_EARGC);
		if (!isflt(av[1]))
			return(MG_ETYPE);
		c_cmaterial->ed = atof(av[1]);
		if (c_cmaterial->ed < 0.)
			return(MG_EILL);
		c_cmaterial->ed_c = *c_ccolor;
		c_cmaterial->clock++;
		return(MG_OK);
	case MG_E_TD:		/* set diffuse transmittance */
		if (ac != 2)
			return(MG_EARGC);
		if (!isflt(av[1]))
			return(MG_ETYPE);
		c_cmaterial->td = atof(av[1]);
		if (c_cmaterial->td < 0. | c_cmaterial->td > 1.)
			return(MG_EILL);
		c_cmaterial->td_c = *c_ccolor;
		return(MG_OK);
	case MG_E_RS:		/* set specular reflectance */
		if (ac != 3)
			return(MG_EARGC);
		if (!isflt(av[1]) || !isflt(av[2]))
			return(MG_ETYPE);
		c_cmaterial->rs = atof(av[1]);
		c_cmaterial->rs_a = atof(av[2]);
		if (c_cmaterial->rs < 0. | c_cmaterial->rs > 1. |
				c_cmaterial->rs_a < 0.)
			return(MG_EILL);
		c_cmaterial->rs_c = *c_ccolor;
		c_cmaterial->clock++;
		return(MG_OK);
	case MG_E_TS:		/* set specular transmittance */
		if (ac != 3)
			return(MG_EARGC);
		if (!isflt(av[1]) || !isflt(av[2]))
			return(MG_ETYPE);
		c_cmaterial->ts = atof(av[1]);
		c_cmaterial->ts_a = atof(av[2]);
		if (c_cmaterial->ts < 0. | c_cmaterial->ts > 1. |
				c_cmaterial->ts_a < 0.)
			return(MG_EILL);
		c_cmaterial->ts_c = *c_ccolor;
		c_cmaterial->clock++;
		return(MG_OK);
	}
	return(MG_EUNK);
}


int
c_hvertex(ac, av)		/* handle a vertex entity */
int	ac;
register char	**av;
{
	register LUENT	*lp;

	switch (mg_entity(av[0])) {
	case MG_E_VERTEX:	/* get/set vertex context */
		if (ac == 1) {		/* set unnamed vertex context */
			c_unvertex = c_dfvertex;
			c_cvertex = &c_unvertex;
			return(MG_OK);
		}
		lp = lu_find(&vtx_tab, av[1]);	/* lookup context */
		if (lp == NULL)
			return(MG_EMEM);
		if (ac == 2) {		/* reestablish previous context */
			if (lp->data == NULL)
				return(MG_EUNDEF);
			c_cvertex = (C_VERTEX *)lp->data;
			return(MG_OK);
		}
		if (av[2][0] != '=' || av[2][1])
			return(MG_ETYPE);
		if (lp->key == NULL) {	/* create new vertex context */
			lp->key = (char *)malloc(strlen(av[1])+1);
			if (lp->key == NULL)
				return(MG_EMEM);
			strcpy(lp->key, av[1]);
			lp->data = (char *)malloc(sizeof(C_VERTEX));
			if (lp->data == NULL)
				return(MG_EMEM);
		}
		c_cvertex = (C_VERTEX *)lp->data;
		if (ac == 3) {		/* use default template */
			*c_cvertex = c_dfvertex;
			return(MG_OK);
		}
		lp = lu_find(&vtx_tab, av[3]);	/* lookup template */
		if (lp == NULL)
			return(MG_EMEM);
		if (lp->data == NULL)
			return(MG_EUNDEF);
		*c_cvertex = *(C_VERTEX *)lp->data;
		if (ac > 4)
			return(MG_EARGC);
		return(MG_OK);
	case MG_E_POINT:	/* set point */
		if (ac != 4)
			return(MG_EARGC);
		if (!isflt(av[1]) || !isflt(av[2]) || !isflt(av[3]))
			return(MG_ETYPE);
		c_cvertex->p[0] = atof(av[1]);
		c_cvertex->p[1] = atof(av[2]);
		c_cvertex->p[2] = atof(av[3]);
		return(MG_OK);
	case MG_E_NORMAL:	/* set normal */
		if (ac != 4)
			return(MG_EARGC);
		if (!isflt(av[1]) || !isflt(av[2]) || !isflt(av[3]))
			return(MG_ETYPE);
		c_cvertex->n[0] = atof(av[1]);
		c_cvertex->n[1] = atof(av[2]);
		c_cvertex->n[2] = atof(av[3]);
		(void)normalize(c_cvertex->n);
		return(MG_OK);
	}
	return(MG_EUNK);
}


void
c_clearall()			/* empty context tables */
{
	c_uncolor = c_dfcolor;
	c_ccolor = &c_uncolor;
	lu_done(&clr_tab);
	c_unmaterial = c_dfmaterial;
	c_cmaterial = &c_unmaterial;
	lu_done(&mat_tab);
	c_unvertex = c_dfvertex;
	c_cvertex = &c_unvertex;
	lu_done(&vtx_tab);
}


C_VERTEX *
c_getvert(name)			/* get a named vertex */
char	*name;
{
	register LUENT	*lp;

	if ((lp = lu_find(&vtx_tab, name)) == NULL)
		return(NULL);
	return((C_VERTEX *)lp->data);
}
