/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Table lookup routines
 */

#include <stdio.h>
#include "lookup.h"

#ifndef MEM_PTR
#define MEM_PTR		void *
#endif

extern MEM_PTR	calloc();


int
lu_init(tbl, nel)		/* initialize tbl for at least nel elements */
register LUTAB	*tbl;
int	nel;
{
	static int  hsiztab[] = {
		31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 0
	};
	register int  *hsp;

	nel += nel>>1;			/* 66% occupancy */
	for (hsp = hsiztab; *hsp; hsp++)
		if (*hsp > nel)
			break;
	if (!(tbl->tsiz = *hsp))
		tbl->tsiz = nel*2 + 1;		/* not always prime */
	tbl->tabl = (LUENT *)calloc(tbl->tsiz, sizeof(LUENT));
	if (tbl->tabl == NULL)
		tbl->tsiz = 0;
	tbl->ndel = 0;
	return(tbl->tsiz);
}


int
lu_hash(s)			/* hash a nul-terminated string */
register char  *s;
{
	register int  h = 0;

	while (*s)
		h = (h<<1 & 0x7fff) ^ (*s++ & 0xff);
	return(h);
}


LUENT *
lu_find(tbl, key)		/* find a table entry */
register LUTAB	*tbl;
char	*key;
{
	int  hval, i;
	register int  ndx;
	LUENT  *oldtabl;
					/* look up object */
	hval = lu_hash(key);
tryagain:
	for (i = 0; i < tbl->tsiz; i++) {
		ndx = (hval + i*i) % tbl->tsiz;
		if (tbl->tabl[ndx].key == NULL ||
				!strcmp(tbl->tabl[ndx].key, key))
			return(&tbl->tabl[ndx]);
	}
					/* table is full, reallocate */
	oldtabl = tbl->tabl;
	ndx = tbl->tsiz;
	i = tbl->ndel;
	if (!lu_init(tbl, ndx-i)) {	/* no more memory! */
		tbl->tabl = oldtabl;
		tbl->tsiz = ndx;
		tbl->ndel = i;
		return(NULL);
	}
	if (!ndx)
		goto tryagain;
	while (ndx--)
		if (oldtabl[ndx].key != NULL)
			if (oldtabl[ndx].data != NULL)
				*lu_find(tbl, oldtabl[ndx].key) = oldtabl[ndx];
			else if (tbl->freek != NULL)
				(*tbl->freek)(oldtabl[ndx].key);
	free((MEM_PTR)oldtabl);
	goto tryagain;			/* should happen only once! */
}


void
lu_delete(tbl, key)		/* delete a table entry */
register LUTAB	*tbl;
char	*key;
{
	register LUENT	*le;

	if ((le = lu_find(tbl, key)) == NULL)
		return;
	if (le->key == NULL || le->data == NULL)
		return;
	if (tbl->freed != NULL)
		(*tbl->freed)(le->data);
	le->data = NULL;
	tbl->ndel++;
}


void
lu_done(tbl)			/* free table and contents */
register LUTAB	*tbl;
{
	register LUENT	*tp;

	if (!tbl->tsiz)
		return;
	for (tp = tbl->tabl + tbl->tsiz; tp-- > tbl->tabl; )
		if (tp->key != NULL) {
			if (tbl->freek != NULL)
				(*tbl->freek)(tp->key);
			if (tp->data != NULL && tbl->freed != NULL)
				(*tbl->freed)(tp->data);
		}
	free((MEM_PTR)tbl->tabl);
	tbl->tabl = NULL;
	tbl->tsiz = 0;
	tbl->ndel = 0;
}
