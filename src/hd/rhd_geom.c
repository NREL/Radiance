/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Octree drawing operations for OpenGL driver.
 */

#include "radogl.h"

#ifndef MAXOCTREE
#define	MAXOCTREE	8		/* maximum octrees to load at once */
#endif

static struct otEntry {
	char	*otfile;		/* octree file name */
	FVECT	cent;			/* centroid */
	FLOAT	rad;			/* radius */
	int	listid;			/* display list identifier */
} otCurrent[MAXOCTREE], otNext[MAXOCTREE];	/* current and next list */

#define FORALLOCT(ot,i)		for (i=0;i<MAXOCTREE&&ot[i].otfile!=NULL;i++)


otNewOctree(fname)		/* add new octree to next list */
char	*fname;
{
	register int	i, j;
					/* check if already in next list */
	FORALLOCT(otNext, i)
		if (!strcmp(fname, otNext[i].otfile))
			return;
	if (i >= MAXOCTREE) {
		error(WARNING, "too many section octrees -- ignoring extra");
		return;
	}
					/* check if copy in current list */
	FORALLOCT(otCurrent, j)
		if (!strcmp(fname, otCurrent[j].otfile)) {
			copystruct(&otNext[i], &otCurrent[i]);
			return;
		}
					/* else load new octree */
	otNext[i].otfile = fname;
	dolights = 0;
	otNext[i].listid = rgl_octlist(fname, otNext[i].cent, &otNext[i].rad);
#ifdef DEBUG
	fprintf(stderr, "Loaded octree \"%s\" into listID %d with radius %f\n",
			fname, otNext[i].listid, otNext[i].rad);
#endif
}


otEndOctree()			/* make next list current */
{
	register int	i, j;

	FORALLOCT(otCurrent, i) {
		FORALLOCT(otNext, j)
			if (otNext[j].listid == otCurrent[i].listid)
				break;
		if (j >= MAXOCTREE || otNext[j].otfile == NULL)
			glDeleteLists(otCurrent[i].listid, 1);	/* not found */
	}
	bcopy((char *)otNext, (char *)otCurrent, sizeof(otNext));
	otNext[0].otfile = NULL;
}


int
otDrawOctrees()			/* draw current list of octrees */
{
	register int	i;

	FORALLOCT(otCurrent, i)
		glCallList(otCurrent[i].listid);
	return(i);
}


otDepthLimit(dl, vorg, vdir)	/* compute approximate depth limits for view */
double	dl[2];
FVECT	vorg, vdir;
{
	FVECT	v;
	double	dcent;
	register int	i;

	dl[0] = FHUGE; dl[1] = 0.;
	FORALLOCT(otCurrent, i) {
		VSUB(v, otCurrent[i].cent, vorg);
		dcent = DOT(v, vdir);
		if (dl[0] > dcent-otCurrent[i].rad)
			dl[0] = dcent-otCurrent[i].rad;
		if (dl[1] < dcent+otCurrent[i].rad)
			dl[1] = dcent+otCurrent[i].rad;
	}
	if (dl[0] < 0.)
		dl[0] = 0.;
}
