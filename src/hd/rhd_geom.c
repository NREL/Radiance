/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Geometry drawing operations for OpenGL driver.
 */

#include "radogl.h"
#include "rhdriver.h"

#ifndef MAXGEO
#define	MAXGEO		8		/* maximum geometry list length */
#endif
#ifndef MAXPORT
#define MAXPORT		(MAXGEO*4)	/* maximum number of portal files */
#endif

int	gmPortals = 0;			/* current portal GL list id */
static char	*curportlist[MAXPORT];	/* current portal list */
static char	*newportlist[MAXPORT];	/* new portal file list */

static struct gmEntry {
	char	*gfile;			/* geometry file name */
	FVECT	cent;			/* centroid */
	FLOAT	rad;			/* radius */
	int	listid;			/* display list identifier */
} gmCurrent[MAXGEO], gmNext[MAXGEO];	/* current and next list */

#define FORALLGEOM(ot,i)	for (i=0;i<MAXGEO&&ot[i].gfile!=NULL;i++)

#define FORALLPORT(pl,i)		for (i=0;i<MAXPORT&&pl[i]!=NULL;i++)

extern char	*atos(), *sskip(), *sskip2();


gmNewGeom(file)			/* add new geometry to next list */
char	*file;
{
	register int	i, j;
					/* check if already in next list */
	FORALLGEOM(gmNext, i)
		if (!strcmp(file, gmNext[i].gfile))
			return;
	if (i >= MAXGEO) {
		error(WARNING, "too many section octrees -- ignoring extra");
		return;
	}
					/* check if copy in current list */
	FORALLGEOM(gmCurrent, j)
		if (!strcmp(file, gmCurrent[j].gfile)) {
			copystruct(&gmNext[i], &gmCurrent[j]);
			return;
		}
					/* else load new octree */
	gmNext[i].gfile = file;
	dolights = 0;
	domats = 1;
	gmNext[i].listid = rgl_octlist(file, gmNext[i].cent, &gmNext[i].rad);
#ifdef DEBUG
	fprintf(stderr, "Loaded octree \"%s\" into listID %d with radius %f\n",
			file, gmNext[i].listid, gmNext[i].rad);
#endif
}


gmEndGeom()			/* make next list current */
{
	register int	i, j;

	FORALLGEOM(gmCurrent, i) {
		FORALLGEOM(gmNext, j)
			if (gmNext[j].listid == gmCurrent[i].listid)
				break;
		if (j >= MAXGEO || gmNext[j].gfile == NULL)
			glDeleteLists(gmCurrent[i].listid, 1);	/* not found */
	}
	bcopy((char *)gmNext, (char *)gmCurrent, sizeof(gmNext));
	bzero((char *)gmNext, sizeof(gmNext));
}


int
gmDrawGeom(clearports)		/* draw current list of octrees (and ports) */
int	clearports;
{
	register int	n;

	FORALLGEOM(gmCurrent, n)
		glCallList(gmCurrent[n].listid);
	if (!n | !clearports | !gmPortals)
		return(n);
				/* mark alpha channel over the portals */
	glPushAttrib(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	glDepthMask(GL_FALSE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT);
	glColor4ub(0, 0, 0, 0xff);	/* write alpha buffer as mask */
	glCallList(gmPortals);		/* draw them portals */
	glPopAttrib();
	return(n);
}


gmDepthLimit(dl, vorg, vdir)	/* compute approximate depth limits for view */
double	dl[2];
FVECT	vorg, vdir;
{
	FVECT	v;
	double	dcent;
	register int	i;

	dl[0] = FHUGE; dl[1] = 0.;
	FORALLGEOM(gmCurrent, i) {
		VSUB(v, gmCurrent[i].cent, vorg);
		dcent = DOT(v, vdir);
		if (dl[0] > dcent-gmCurrent[i].rad)
			dl[0] = dcent-gmCurrent[i].rad;
		if (dl[1] < dcent+gmCurrent[i].rad)
			dl[1] = dcent+gmCurrent[i].rad;
	}
	if (dl[0] < 0.)
		dl[0] = 0.;
}


gmNewPortal(pflist)		/* add portal file(s) to our new list */
char	*pflist;
{
	register int	i, j;
	char	newfile[128];

	if (pflist == NULL)
		return;
	while (*pflist) {
		atos(newfile, sizeof(newfile), pflist);
		if (!*newfile)
			break;
		pflist = sskip(pflist);
		FORALLPORT(newportlist,i)
			if (!strcmp(newportlist[i], newfile))
				goto endloop;	/* in list already */
		if (i >= MAXPORT) {
			error(WARNING, "too many portals -- ignoring extra");
			return;
		}
		newportlist[i] = savestr(newfile);
	endloop:;
	}
}


static int
sstrcmp(ss0, ss1)
char	**ss0, **ss1;
{
	return(strcmp(*ss0, *ss1));
}


int
gmEndPortal()			/* close portal list and return GL list */
{
	register int	n;

	FORALLPORT(newportlist, n);
	if (!n) {			/* free old GL list */
		if (gmPortals)
			glDeleteLists(gmPortals, 1);
		gmPortals = 0;
	} else
		qsort(newportlist, n, sizeof(char *), sstrcmp);
	FORALLPORT(newportlist, n)		/* compare sorted lists */
		if (curportlist[n] == NULL ||
				strcmp(curportlist[n],newportlist[n])) {
						/* load new list */
			if (gmPortals)
				glDeleteLists(gmPortals, 1);
			FORALLPORT(newportlist, n);
			dolights = 0;
			domats = 0;
			gmPortals = rgl_filelist(n, newportlist);
			break;
		}
	FORALLPORT(curportlist, n)		/* free old file list */
		freestr(curportlist[n]);
	bcopy((char *)newportlist, (char *)curportlist, sizeof(newportlist));
	bzero((char *)newportlist, sizeof(newportlist));
	return(gmPortals);			/* return GL list id */
}
