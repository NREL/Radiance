/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  objset.c - routines for maintaining object sets.
 *
 *	7/28/85
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "otypes.h"

#define  OSTSIZ		3037		/* object table size (a prime!) */

static OBJECT  *ostable[OSTSIZ];	/* the object set table */


insertelem(os, obj)		/* insert obj into os, no questions */
register OBJECT  *os;
OBJECT  obj;
{
	register int  i;
	
	for (i = os[0]++; i > 0; i--)
		if (os[i] > obj)
			os[i+1] = os[i];
		else
			break;
	os[i+1] = obj;
}


deletelem(os, obj)		/* delete obj from os, no questions */
register OBJECT  *os;
OBJECT  obj;
{
	register int  i;

	for (i = (*os++)--; i > 0 && *os < obj; i--, os++)
		;
	while (--i > 0) {
		os[0] = os[1];
		os++;
	}
}


inset(os, obj)			/* determine if object is in set */
register OBJECT  *os;
OBJECT  obj;
{
	int  upper, lower;
	register int  cm, i;

	lower = 1;
	upper = cm = os[0] + 1;

	while ((i = (lower + upper) >> 1) != cm) {
		cm = obj - os[i];
		if (cm > 0)
			lower = i;
		else if (cm < 0)
			upper = i;
		else
			return(1);
		cm = i;
	}
	return(0);
}


setequal(os1, os2)		/* determine if two sets are equal */
register OBJECT  *os1, *os2;
{
	register int  i;

	for (i = *os1; i-- >= 0; )
		if (*os1++ != *os2++)
			return(0);
	return(1);
}


setcopy(os1, os2)		/* copy object set os2 into os1 */
register OBJECT  *os1, *os2;
{
	register int  i;

	for (i = *os2; i-- >= 0; )
		*os1++ = *os2++;
}


OCTREE
fullnode(oset)			/* return octree for object set */
OBJECT  *oset;
{
	int  osentry, ntries;
	long  hval;
	OCTREE  ot;
	register int  i;
	register OBJECT  *os;
					/* hash on set */
	hval = 0;
	os = oset;
	i = *os++;
	while (i-- > 0)
		hval += *os++;
	ntries = 0;
tryagain:
	osentry = (hval + (long)ntries*ntries) % OSTSIZ;
	os = ostable[osentry];
	if (os == NULL) {
		os = ostable[osentry] = (OBJECT *)malloc(
				(unsigned)(oset[0]+2)*sizeof(OBJECT));
		if (os == NULL)
			goto memerr;
		ot = oseti(osentry);
	} else {
						/* look for set */
		for (i = 0; *os > 0; i++, os += *os + 1)
			if (setequal(os, oset))
				break;
		ot = oseti(i*OSTSIZ + osentry);
		if (*os > 0)			/* found it */
			return(ot);
		if (!isfull(ot))		/* entry overflow */
			if (++ntries < OSTSIZ)
				goto tryagain;
			else
				error(INTERNAL, "hash table overflow in fullnode");
						/* remember position */
		i = os - ostable[osentry];
		os = ostable[osentry] = (OBJECT *)realloc(
				(char *)ostable[osentry],
				(unsigned)(i+oset[0]+2)*sizeof(OBJECT));
		if (os == NULL)
			goto memerr;
		os += i;			/* last entry */
	}
	setcopy(os, oset);		/* add new set */
	os += *os + 1;
	*os = 0;			/* terminator */
	return(ot);
memerr:
	error(SYSTEM, "out of memory in fullnode");
}


objset(oset, ot)		/* get object set for full node */
register OBJECT  *oset;
OCTREE  ot;
{
	register OBJECT  *os;
	register int  i;

	if (!isfull(ot))
		goto noderr;
	i = oseti(ot);
	if ((os = ostable[i%OSTSIZ]) == NULL)
		goto noderr;
	for (i /= OSTSIZ; i--; os += *os + 1)
		if (*os <= 0)
			goto noderr;
	for (i = *os; i-- >= 0; )		/* copy set here */
		*oset++ = *os++;
	return;
noderr:
	error(CONSISTENCY, "bad node in objset");
}


nonsurfinset(orig, nobjs)		/* check sets for non-surfaces */
int  orig, nobjs;
{
	int  n;
	OBJECT  *nonset;
	register OBJECT  *os;
	register OBJECT  i;
						/* count non-surfaces */
	n = 0;
	for (i = orig; i < orig+nobjs; i++)
		if (!issurface(objptr(i)->otype))
			n++;
	if (n <= 0)
		return(0);
						/* allocate set */
	if ((nonset = (OBJECT *)malloc((n+1)*sizeof(OBJECT))) == NULL)
		return(0);		/* give up if we haven't enough mem */
						/* fill set */
	os = nonset;
	*os = n;
	for (i = orig; i < orig+nobjs; i++)
		if (!issurface(objptr(i)->otype))
			*++os = i;
						/* now check all sets */
	for (n = 0; n < OSTSIZ; n++) {
		if ((os = ostable[n]) == NULL)
			continue;
		while ((i = *os++) > 0)
			while (i--) {
				if (*os >= nonset[1]
						&& *os <= nonset[nonset[0]]
						&& inset(nonset, *os))
					goto done;
				os++;
			}
	}
done:
	free((char *)nonset);
	return(n < OSTSIZ);
}
