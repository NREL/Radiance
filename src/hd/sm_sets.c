/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Quadtree-specific set operations.
 */

#include "standard.h"

#include "sm_qtree.h"
#include "object.h"

#define QTSETIBLK	1024		/* set index allocation block size */
#define QTELEMBLK	8		/* set element allocation block */

#define QTNODESIZ(ne)	((ne) + QTELEMBLK - ((ne) % QTELEMBLK))

static OBJECT	**qtsettab;		/* quadtree leaf node table */
static QUADTREE  qtnumsets;		/* number of used set indices */
static int  qtfreesets = EMPTY;		/* free set index list */


QUADTREE
qtnewleaf(oset)			/* return new leaf node for object set */
OBJECT  *oset;
{
	register QUADTREE  osi;
	int	nalc;

	if (*oset <= 0)
		return(EMPTY);		/* should be error? */
	if (qtfreesets != EMPTY) {
		osi = qtfreesets;
		qtfreesets = (int)qtsettab[osi];
	} else if ((osi = qtnumsets++) % QTSETIBLK == 0) {
		qtsettab = (OBJECT **)realloc((char *)qtsettab,
				(unsigned)(osi+QTSETIBLK)*sizeof(OBJECT *));
		if (qtsettab == NULL)
			goto memerr;
	}
	nalc = QTNODESIZ(*oset);
	if ((qtsettab[osi] = (OBJECT *)malloc(nalc*sizeof(OBJECT))) == NULL)
		goto memerr;
	setcopy(qtsettab[osi], oset);
	return(QT_INDEX(osi));
memerr:
	error(SYSTEM, "out of memory in qtnewleaf");
}


QUADTREE
qtdelelem(qt, id)		/* delete element from leaf node, no quest. */
QUADTREE  qt;
OBJECT  id;
{
	register QUADTREE  lf;
	int  oalc, nalc;

	if (QT_IS_EMPTY(qt))
		return(EMPTY);
	if (!QT_IS_LEAF(qt))
		goto noderr;
	lf = QT_SET_INDEX(qt);
	if (lf >= qtnumsets)
		goto noderr;
	if (qtsettab[lf][0] <= 1) {		/* blow leaf away */
		free((char *)qtsettab[lf]);
		qtsettab[lf] = (OBJECT *)qtfreesets;
		qtfreesets = lf;
		return(EMPTY);
	}					/* else delete element */
	oalc = QTNODESIZ(qtsettab[lf][0]);
	nalc = QTNODESIZ(qtsettab[lf][0] - 1);
	deletelem(qtsettab[lf], id);
	if (nalc != oalc)
		qtsettab[lf] = (OBJECT *)realloc((char *)qtsettab[lf],
				nalc*sizeof(OBJECT));
	return(qt);
noderr:
	error(CONSISTENCY, "bad node in qtdelelem");
}


QUADTREE
qtaddelem(qt, id)		/* add element to leaf node, no quest. */
QUADTREE  qt;
OBJECT  id;
{
	OBJECT	newset[2];
	register QUADTREE  lf;
	int  oalc, nalc;

	if (QT_IS_EMPTY(qt)) {		/* create new leaf */
		newset[0] = 1; newset[1] = id;
		return(qtnewleaf(newset));
	}				/* else add element */
	if (!QT_IS_LEAF(qt))
		goto noderr;
	lf = QT_SET_INDEX(qt);
	if (lf >= qtnumsets)
		goto noderr;
	oalc = QTNODESIZ(qtsettab[lf][0]);
	nalc = QTNODESIZ(qtsettab[lf][0] + 1);
	if (nalc != oalc) {
		qtsettab[lf] = (OBJECT *)realloc((char *)qtsettab[lf],
				nalc*sizeof(OBJECT));
		if (qtsettab[lf] == NULL)
			error(SYSTEM, "out of memory in qtaddelem");
	}
	insertelem(qtsettab[lf], id);
	return(qt);
noderr:
	error(CONSISTENCY, "bad node in qtaddelem");
}


qtgetset(oset, qt)		/* get object set for leaf node */
register OBJECT  *oset;
QUADTREE  qt;
{
	QUADTREE  lf;
	register OBJECT  *os;
	register int  i;

	if (!QT_IS_LEAF(qt))
		goto noderr;
	lf = QT_SET_INDEX(qt);
	if (lf >= qtnumsets)
		goto noderr;
	for (i = *(os = qtsettab[lf]); i-- >= 0; )
		*oset++ = *os++;
	return;
noderr:
	error(CONSISTENCY, "bad node in qtgetset");
}


qtfreeleaf(qt)			/* free set and leaf node */
QUADTREE  qt;
{
	register QUADTREE	osi;

	if (!QT_IS_LEAF(qt))
		return;
	osi = QT_SET_INDEX(qt);
	if (osi >= qtnumsets)
		error(CONSISTENCY, "bad node in qtfreeleaf");
	free((char *)qtsettab[osi]);
	qtsettab[osi] = (OBJECT *)qtfreesets;
	qtfreesets = osi;
}


check_set(os, cs)                /* modify checked set and set to check */
register OBJECT  *os;                   /* os' = os - cs */
register OBJECT  *cs;                   /* cs' = cs + os */
{
    OBJECT  cset[MAXCSET+MAXSET+1];
    register int  i, j;
    int  k;
                                        /* copy os in place, cset <- cs */
    cset[0] = 0;
    k = 0;
    for (i = j = 1; i <= os[0]; i++) {
	while (j <= cs[0] && cs[j] < os[i])
	   cset[++cset[0]] = cs[j++];
	if (j > cs[0] || os[i] != cs[j]) {      /* object to check */
	    os[++k] = os[i];
	    cset[++cset[0]] = os[i];
	}
    }
    if (!(os[0] = k))               /* new "to check" set size */
       return;                 /* special case */
    while (j <= cs[0])              /* get the rest of cs */
       cset[++cset[0]] = cs[j++];
    if (cset[0] > MAXCSET)          /* truncate "checked" set if nec. */
       cset[0] = MAXCSET;
    /* setcopy(cs, cset); */        /* copy cset back to cs */
    os = cset;
    for (i = os[0]; i-- >= 0; )
       *cs++ = *os++;
}
