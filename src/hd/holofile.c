/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Routines for managing holodeck files
 *
 *	9/30/97	GWLarson
 */

#include "holo.h"

#ifndef CACHESIZE
#define CACHESIZE	16	/* default cache size (Mbytes, 0==inf) */
#endif
#ifndef FREEBEAMS
#define FREEBEAMS	512	/* maximum beams to free at a time */
#endif
#ifndef PCTFREE
#define PCTFREE		20	/* maximum fraction to free (%) */
#endif

#define MAXFRAG		(128*FRAGBLK)	/* maximum fragments per file */

#define FRAGBLK		64	/* number of fragments to allocate at a time */

int	hdcachesize = CACHESIZE*1024*1024;	/* target cache size (bytes) */
unsigned long	hdclock;	/* clock value */

HOLO	*hdlist[HDMAX+1];	/* holodeck pointers (NULL term.) */

static struct fragment {
	short	nlinks;		/* number of holodeck sections using us */
	short	nfrags;		/* number of known fragments */
	BEAMI	*fi;		/* fragments, descending file position */
	long	flen;		/* last known file length */
} *hdfrag;		/* fragment lists, indexed by file descriptor */

static int	nhdfrags;	/* size of hdfrag array */


hdattach(fd)		/* start tracking file fragments for some section */
register int	fd;
{
	if (fd >= nhdfrags) {
		if (nhdfrags)
			hdfrag = (struct fragment *)realloc((char *)hdfrag,
					(fd+1)*sizeof(struct fragment));
		else
			hdfrag = (struct fragment *)malloc(
					(fd+1)*sizeof(struct fragment));
		if (hdfrag == NULL)
			error(SYSTEM, "out of memory in hdattach");
		bzero((char *)(hdfrag+nhdfrags),
				(fd+1-nhdfrags)*sizeof(struct fragment));
		nhdfrags = fd+1;
	}
	hdfrag[fd].nlinks++;
	hdfrag[fd].flen = lseek(fd, 0L, 2);	/* get file length */
}


/* Do we need a routine to locate file fragments given known occupants? */


hdrelease(fd)		/* stop tracking file fragments for some section */
register int	fd;
{
	if (fd >= nhdfrags || !hdfrag[fd].nlinks)
		return;
	if (!--hdfrag[fd].nlinks && hdfrag[fd].nfrags) {
		free((char *)hdfrag[fd].fi);
		hdfrag[fd].fi = NULL;
		hdfrag[fd].nfrags = 0;
	}
}


HOLO *
hdinit(fd, hproto)	/* initialize a holodeck section in a file */
int	fd;			/* corresponding file descriptor */
HDGRID	*hproto;		/* holodeck section grid */
{
	long	fpos;
	register HOLO	*hp;
	register int	n;
					/* prepare for system errors */
	errno = 0;
	if ((fpos = lseek(fd, 0L, 1)) < 0)
		error(SYSTEM, "cannot determine holodeck file position");
	if (hproto == NULL) {		/* assume we're loading it */
		HDGRID	hpr;
						/* load header */
		if (read(fd, (char *)&hpr, sizeof(HDGRID)) != sizeof(HDGRID))
			error(SYSTEM, "cannot load holodeck header");
						/* allocate grid */
		if ((hp = hdalloc(&hpr)) == NULL)
			goto memerr;
						/* load beam directory */
		n = nbeams(hp)*sizeof(BEAMI);
		if (read(fd, (char *)(hp->bi+1), n) != n)
			error(SYSTEM, "failure loading holodeck directory");
	} else {			/* assume we're creating it */
		if ((hp = hdalloc(hproto)) == NULL)
			goto memerr;
						/* write header and skeleton */
		n = nbeams(hp)*sizeof(BEAMI);
		if (write(fd, (char *)hproto, sizeof(HDGRID)) !=
					sizeof(HDGRID) ||
				write(fd, (char *)(hp->bi+1), n) != n)
			error(SYSTEM, "cannot write header to holodeck file");
	}
	hp->fd = fd;	
	hp->dirty = 0;
	biglob(hp)->fo = fpos + sizeof(HDGRID);
	biglob(hp)->nrd = 0;		/* count rays on disk */
	for (n = nbeams(hp); n > 0; n--)
		biglob(hp)->nrd += hp->bi[n].nrd;
					/* add to holodeck list */
	for (n = 0; n < HDMAX; n++)
		if (hdlist[n] == NULL) {
			hdlist[n] = hp;
			break;
		}
					/* start tracking fragments (last) */
	hdattach(fd);
					/* all done */
	return(hp);
memerr:
	error(SYSTEM, "cannot allocate holodeck grid");
}


int
hdsync(hp)			/* update directory on disk if necessary */
register HOLO	*hp;
{
	register int	j, n;

	if (hp == NULL) {		/* do all */
		n = 0;
		for (j = 0; hdlist[j] != NULL; j++)
			n += hdsync(hdlist[j]);
		return(n);
	}
	if (!hp->dirty)			/* check first */
		return(0);
	errno = 0;
	if (lseek(hp->fd, biglob(hp)->fo, 0) < 0)
		error(SYSTEM, "cannot seek on holodeck file");
	n = nbeams(hp)*sizeof(BEAMI);
	if (write(hp->fd, (char *)(hp->bi+1), n) != n)
		error(SYSTEM, "cannot update holodeck section directory");
	hp->dirty = 0;
	return(1);
}


long
hdmemuse(all)		/* return memory usage (in bytes) */
int	all;			/* include overhead (painful) */
{
	long	total = 0;
	register int	i, j;

	for (j = 0; hdlist[j] != NULL; j++) {
		total += blglob(hdlist[j])->nrm * sizeof(RAYVAL);
		if (all) {
			total += sizeof(HOLO) + sizeof(BEAM *) +
					nbeams(hdlist[j]) *
						(sizeof(BEAM *)+sizeof(BEAMI));
			for (i = nbeams(hdlist[j]); i > 0; i--)
				if (hdlist[j]->bl[i] != NULL)
					total += sizeof(BEAM);
		}
	}
	if (all)
		for (j = 0; j < nhdfrags; j++) {
			total += sizeof(struct fragment);
			if (hdfrag[j].nfrags)
				total += FRAGBLK*sizeof(BEAMI) *
					((hdfrag[j].nfrags-1)/FRAGBLK + 1) ;
		}
	return(total);
}


long
hdfiluse(fd, all)	/* compute file usage (in bytes) */
int	fd;			/* open file descriptor to check */
int	all;			/* include overhead and unflushed data */
{
	long	total = 0;
	register int	i, j;

	for (j = 0; hdlist[j] != NULL; j++) {
		if (hdlist[j]->fd != fd)
			continue;
		total += biglob(hdlist[j])->nrd * sizeof(RAYVAL);
		if (all) {
			for (i = nbeams(hdlist[j]); i > 0; i--)
				if (hdlist[j]->bl[i] != NULL)
					total += sizeof(RAYVAL) *
						(hdlist[j]->bl[i]->nrm -
						hdlist[j]->bi[i].nrd);
			total += sizeof(HDGRID) +
					nbeams(hdlist[j])*sizeof(BEAMI);
		}
	}
	return(total);		/* does not include fragments */
}


RAYVAL *
hdnewrays(hp, i, nr)	/* allocate space for add'l rays and return pointer */
register HOLO	*hp;
register int	i;
int	nr;			/* number of new rays desired */
{
	RAYVAL	*p;
	int	n;

	if (nr <= 0)
		return(NULL);
	if (i < 1 | i > nbeams(hp))
		error(CONSISTENCY, "bad beam index given to hdnewrays");
	if (hp->bl[i] != NULL)
		hp->bl[i]->tick = hdclock;	/* preempt swap */
	if (hdcachesize > 0 && hdmemuse(0) >= hdcachesize)
		hdfreecache(PCTFREE, NULL);	/* free some space */
	errno = 0;
	if (hp->bl[i] == NULL) {		/* allocate (and load) */
		n = hp->bi[i].nrd + nr;
		if ((hp->bl[i] = (BEAM *)malloc(hdbsiz(n))) == NULL)
			goto memerr;
		blglob(hp)->nrm += n;
		if (n = hp->bl[i]->nrm = hp->bi[i].nrd) {
			if (lseek(hp->fd, hp->bi[i].fo, 0) < 0)
				error(SYSTEM, "seek error on holodeck file");
			n *= sizeof(RAYVAL);
			if (read(hp->fd, (char *)hdbray(hp->bl[i]), n) != n)
				error(SYSTEM,
				"error reading beam from holodeck file");
		}
	} else {				/* just grow in memory */
		hp->bl[i] = (BEAM *)realloc( (char *)hp->bl[i],
				hdbsiz(hp->bl[i]->nrm + nr) );
		if (hp->bl[i] == NULL)
			goto memerr;
		blglob(hp)->nrm += nr;
	}
	p = hdbray(hp->bl[i]) + hp->bl[i]->nrm;
	hp->bl[i]->nrm += nr;			/* update in-core structure */
	bzero((char *)p, nr*sizeof(RAYVAL));
	hp->bl[i]->tick = ++hdclock;		/* update LRU clock */
	blglob(hp)->tick = hdclock;
	return(p);				/* point to new rays */
memerr:
	error(SYSTEM, "out of memory in hdnewrays");
}


BEAM *
hdgetbeam(hp, i)	/* get beam (from file if necessary) */
register HOLO	*hp;
register int	i;
{
	register int	n;

	if (i < 1 | i > nbeams(hp))
		error(CONSISTENCY, "bad beam index given to hdgetbeam");
	if (hp->bl[i] == NULL) {		/* load from disk */
		if (!(n = hp->bi[i].nrd))
			return(NULL);
		if (hdcachesize > 0 && hdmemuse(0) >= hdcachesize)
			hdfreecache(PCTFREE, NULL);	/* get free space */
		errno = 0;
		if ((hp->bl[i] = (BEAM *)malloc(hdbsiz(n))) == NULL)
			error(SYSTEM, "cannot allocate memory for beam");
		blglob(hp)->nrm += hp->bl[i]->nrm = n;
		if (lseek(hp->fd, hp->bi[i].fo, 0) < 0)
			error(SYSTEM, "seek error on holodeck file");
		n *= sizeof(RAYVAL);
		if (read(hp->fd, (char *)hdbray(hp->bl[i]), n) != n)
			error(SYSTEM, "error reading beam from holodeck file");
	}
	hp->bl[i]->tick = ++hdclock;	/* update LRU clock */
	blglob(hp)->tick = hdclock;
	return(hp->bl[i]);
}


int
hdgetbi(hp, i)			/* allocate a file fragment */
register HOLO	*hp;
register int	i;
{
	int	nrays = hp->bl[i]->nrm;

	if (hp->bi[i].nrd == nrays)	/* current one will do? */
		return(0);

	if (hp->fd >= nhdfrags || !hdfrag[hp->fd].nlinks) /* untracked */
		hp->bi[i].fo = lseek(hp->fd, 0L, 2);

	else if (hp->bi[i].fo + hp->bi[i].nrd*sizeof(RAYVAL) ==
			hdfrag[hp->fd].flen)		/* EOF special case */
		hdfrag[hp->fd].flen = hp->bi[i].fo + nrays*sizeof(RAYVAL);

	else {						/* general case */
		register struct fragment	*f = &hdfrag[hp->fd];
		register int	j, k;
					/* relinquish old fragment */
		if (hp->bi[i].nrd) {
			j = f->nfrags++;
#ifdef MAXFRAG
			if (j >= MAXFRAG-1)
				f->nfrags--;
#endif
			if (j % FRAGBLK == 0) {		/* more frag. space */
				if (f->fi == NULL)
					f->fi = (BEAMI *)malloc(
							FRAGBLK*sizeof(BEAMI));
				else
					f->fi = (BEAMI *)realloc((char *)f->fi,
						(j+FRAGBLK)*sizeof(BEAMI));
				if (f->fi == NULL)
					error(SYSTEM,
						"out of memory in hdgetbi");
			}
			for ( ; ; j--) {	/* insert in descending list */
				if (!j || hp->bi[i].fo < f->fi[j-1].fo) {
					f->fi[j].fo = hp->bi[i].fo;
					f->fi[j].nrd = hp->bi[i].nrd;
					break;
				}
				copystruct(f->fi+j, f->fi+j-1);
			}
					/* coalesce adjacent fragments */
			for (j = k = 0; k < f->nfrags; j++, k++) {
				if (k > j)
					copystruct(f->fi+j, f->fi+k);
				while (k+1 < f->nfrags && f->fi[k+1].fo +
						f->fi[k+1].nrd*sizeof(RAYVAL)
							== f->fi[j].fo) {
					f->fi[j].fo -=
						f->fi[++k].nrd*sizeof(RAYVAL);
					f->fi[j].nrd += f->fi[k].nrd;
				}
			}
			f->nfrags = j;
		}
		k = -1;			/* find closest-sized fragment */
		for (j = f->nfrags; j-- > 0; )
			if (f->fi[j].nrd >= nrays &&
					(k < 0 || f->fi[j].nrd < f->fi[k].nrd))
				if (f->fi[k=j].nrd == nrays)
					break;
		if (k < 0) {		/* no fragment -- extend file */
			hp->bi[i].fo = f->flen;
			f->flen += nrays*sizeof(RAYVAL);
		} else {		/* else use fragment */
			hp->bi[i].fo = f->fi[k].fo;
			if (f->fi[k].nrd == nrays) {	/* delete fragment */
				f->nfrags--;
				for (j = k; j < f->nfrags; j++)
					copystruct(f->fi+j, f->fi+j+1);
			} else {			/* else shrink it */
				f->fi[k].fo += nrays*sizeof(RAYVAL);
				f->fi[k].nrd -= nrays;
			}
		}
	}
	biglob(hp)->nrd += nrays - hp->bi[i].nrd;
	hp->bi[i].nrd = nrays;
	hp->dirty++;		/* section directory now out of date */
	return(1);
}


int
hdfreebeam(hp, i)		/* free beam, writing if dirty */
register HOLO	*hp;
register int	i;
{
	int	nchanged, n;

	if (hp == NULL) {		/* clear all holodecks */
		nchanged = 0;
		for (i = 0; hdlist[i] != NULL; i++)
			nchanged += hdfreebeam(hdlist[i], 0);
		return(nchanged);
	}
	if (i == 0) {			/* clear entire holodeck */
		nchanged = 0;
		for (i = 1; i <= nbeams(hp); i++)
			nchanged += hdfreebeam(hp, i);
		return(nchanged);
	}
	if (i < 1 | i > nbeams(hp))
		error(CONSISTENCY, "bad beam index to hdfreebeam");
	if (hp->bl[i] == NULL)
		return(0);
					/* check for additions */
	nchanged = hp->bl[i]->nrm - hp->bi[i].nrd;
	if (nchanged) {
		hdgetbi(hp, i);			/* allocate a file position */
		errno = 0;
		if (lseek(hp->fd, hp->bi[i].fo, 0) < 0)
			error(SYSTEM, "cannot seek on holodeck file");
		n = hp->bl[i]->nrm * sizeof(RAYVAL);
		if (write(hp->fd, (char *)hdbray(hp->bl[i]), n) != n)
			error(SYSTEM, "write error in hdfreebeam");
	}
	blglob(hp)->nrm -= hp->bl[i]->nrm;
	free((char *)hp->bl[i]);		/* free memory */
	hp->bl[i] = NULL;
	return(nchanged);
}


hdlrulist(ha, ba, n, hp)	/* add beams from holodeck to LRU list */
register HOLO	*ha[];			/* section list (NULL terminated) */
register int	ba[];			/* beam index to go with section */
int	n;				/* length of arrays minus 1 */
register HOLO	*hp;			/* section we're adding from */
{
	register int	i, j;
	int	nents;
					/* find last entry in LRU list */
	for (j = 0; ha[j] != NULL; j++)
		;
	nents = j;
					/* insert each beam from hp */
	for (i = nbeams(hp); i > 0; i-- ) {
		if (hp->bl[i] == NULL)		/* check if loaded */
			continue;
		if ((j = ++nents) > n)		/* grow list if we can */
			nents--;
		for ( ; ; ) {			/* bubble into place */
			if (!--j || hp->bl[i]->tick >=
					ha[j-1]->bl[ba[j-1]]->tick) {
				ha[j] = hp;
				ba[j] = i;
				break;
			}
			ha[j] = ha[j-1];
			ba[j] = ba[j-1];
		}
	}
	ha[nents] = NULL;		/* all done */
	ba[nents] = 0;
}


hdfreecache(pct, honly)		/* free up cache space, writing changes */
int	pct;				/* maximum percentage to free */
register HOLO	*honly;			/* NULL means check all */
{
	HOLO	*hp[FREEBEAMS+1];
	int	bn[FREEBEAMS+1];
	int	freetarget;
	register int	i;
						/* compute free target */
	freetarget = (honly != NULL) ? blglob(honly)->nrm :
			hdmemuse(0)/sizeof(RAYVAL) ;
	freetarget = freetarget*pct/100;
						/* find least recently used */
	hp[0] = NULL;
	bn[0] = 0;
	if (honly != NULL)
		hdlrulist(hp, bn, FREEBEAMS, honly);
	else
		for (i = 0; hdlist[i] != NULL; i++)
			hdlrulist(hp, bn, FREEBEAMS, hdlist[i]);
						/* free LRU beams */
	for (i = 0; hp[i] != NULL; i++) {
		hdfreebeam(hp[i], bn[i]);
		if ((freetarget -= hp[i]->bi[bn[i]].nrd) <= 0)
			break;
	}
	hdsync(honly);		/* synchronize directories as necessary */
}


hddone(hp)		/* clean up holodeck section and free */
register HOLO	*hp;		/* NULL means clean up all */
{
	register int	i;

	if (hp == NULL) {		/* NULL means clean up everything */
		while (hdlist[0] != NULL)
			hddone(hdlist[0]);
		return;
	}
					/* flush all data and free memory */
	hdflush(hp);
					/* release fragment resources */
	hdrelease(hp->fd);
					/* remove hp from active list */
	for (i = 0; hdlist[i] != NULL; i++)
		if (hdlist[i] == hp) {
			while ((hdlist[i] = hdlist[i+1]) != NULL)
				i++;
			break;
		}
	free((char *)hp->bl);		/* free beam list */
	free((char *)hp);		/* free holodeck struct */
}
