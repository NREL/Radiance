#ifndef lint
static const char RCSid[] = "$Id: rc3.c,v 2.24 2020/09/09 21:28:19 greg Exp $";
#endif
/*
 * Accumulate ray contributions for a set of materials
 * Controlling process for multiple children
 */

#include <signal.h>
#include "rcontrib.h"
#include "selcall.h"

#define	MAXIQ		(int)(PIPE_BUF/(sizeof(FVECT)*2))

/* Modifier contribution queue (results waiting to be output) */
typedef struct s_binq {
	RNUMBER		ndx;		/* index for this entry */
	RNUMBER		nadded;		/* accumulated so far */
	struct s_binq	*next;		/* next in queue */
	MODCONT		*mca[1];	/* contrib. array (extends struct) */
} BINQ;

static BINQ	*out_bq = NULL;		/* output bin queue */
static BINQ	*free_bq = NULL;	/* free queue entries */

static SUBPROC	kidpr[MAXPROCESS];	/* our child processes */

static struct {
	RNUMBER	r1;			/* assigned ray starting index */
	FILE	*infp;			/* file pointer to read from process */
	int	nr;			/* number of rays to sum (0 if free) */
} kida[MAXPROCESS];		/* our child process i/o */


/* Get new bin queue entry */
static BINQ *
new_binq()
{
	BINQ	*bp;
	int	i;

	if (free_bq != NULL) {		/* something already available? */
		bp = free_bq;
		free_bq = bp->next;
		bp->next = NULL;
		bp->nadded = 0;
		return(bp);
	}
					/* else allocate fresh */
	bp = (BINQ *)malloc(sizeof(BINQ) + sizeof(MODCONT *)*(nmods-1));
	if (bp == NULL)
		goto memerr;
	for (i = nmods; i--; ) {
		MODCONT	*mp = (MODCONT *)lu_find(&modconttab,modname[i])->data;
		bp->mca[i] = (MODCONT *)malloc(sizeof(MODCONT) +
						sizeof(DCOLOR)*(mp->nbins-1));
		if (bp->mca[i] == NULL)
			goto memerr;
		memcpy(bp->mca[i], mp, sizeof(MODCONT)-sizeof(DCOLOR));
		/* memset(bp->mca[i]->cbin, 0, sizeof(DCOLOR)*mp->nbins); */
	}
	bp->ndx = 0;
	bp->nadded = 0;
	bp->next = NULL;
	return(bp);
memerr:
	error(SYSTEM, "out of memory in new_binq()");
	return(NULL);
}


/* Free a bin queue entry */
static void
free_binq(BINQ *bp)
{
	int	i;

	if (bp == NULL) {		/* signal to release our free list */
		while ((bp = free_bq) != NULL) {
			free_bq = bp->next;
			for (i = nmods; i--; )
				free(bp->mca[i]);
				/* Note: we don't own bp->mca[i]->binv */
			free(bp);
		}
		return;
	}
					/* clear sums for next use */
/*	for (i = nmods; i--; )
		memset(bp->mca[i]->cbin, 0, sizeof(DCOLOR)*bp->mca[i]->nbins);
*/
	if (bp->next != NULL)
		error(CONSISTENCY, "free_binq() handed list");
	bp->ndx = 0;
	bp->next = free_bq;		/* push onto free list */
	free_bq = bp;
}


/* Add modifier values to accumulation record in queue and clear */
static void
queue_modifiers()
{
	MODCONT	*mpin, *mpout;
	int	i, j;

	if ((accumulate > 0) | (out_bq == NULL))
		error(CONSISTENCY, "bad call to queue_modifiers()");

	for (i = nmods; i--; ) {
		mpin = (MODCONT *)lu_find(&modconttab,modname[i])->data;
		mpout = out_bq->mca[i];
		for (j = mpout->nbins; j--; )
			addcolor(mpout->cbin[j], mpin->cbin[j]);
		memset(mpin->cbin, 0, sizeof(DCOLOR)*mpin->nbins);
	}
	out_bq->nadded++;
}


/* Sum one modifier record into another (updates nadded) */
static void
add_modbin(BINQ *dst, BINQ *src)
{
	int	i, j;
	
	for (i = nmods; i--; ) {
		MODCONT	*mpin = src->mca[i];
		MODCONT	*mpout = dst->mca[i];
		for (j = mpout->nbins; j--; )
			addcolor(mpout->cbin[j], mpin->cbin[j]);
	}
	dst->nadded += src->nadded;
}


/* Queue values for later output */
static void
queue_output(BINQ *bp)
{
	BINQ	*b_last, *b_cur;

	if (accumulate <= 0) {		/* just accumulating? */
		if (out_bq == NULL) {
			bp->next = NULL;
			out_bq = bp;
		} else {
			add_modbin(out_bq, bp);
			free_binq(bp);
		}
		return;
	}
	b_last = NULL;			/* insert in output queue */
	for (b_cur = out_bq; b_cur != NULL && b_cur->ndx < bp->ndx;
				b_cur = b_cur->next)
		b_last = b_cur;

	if (b_last != NULL) {
		bp->next = b_cur;
		b_last->next = bp;
	} else {
		bp->next = out_bq;
		out_bq = bp;
	}
	if (accumulate == 1)		/* no accumulation? */
		return;
	b_cur = out_bq;			/* else merge accumulation entries */
	while (b_cur->next != NULL) {
		if (b_cur->nadded >= accumulate ||
				(b_cur->ndx-1)/accumulate !=
				(b_cur->next->ndx-1)/accumulate) {
			b_cur = b_cur->next;
			continue;
		}
		add_modbin(b_cur, b_cur->next);
		b_last = b_cur->next;
		b_cur->next = b_last->next;
		b_last->next = NULL;
		free_binq(b_last);
	}
}


/* Count number of records ready for output */
static int
queue_ready()
{
	int	nready = 0;
	BINQ	*bp;

	for (bp = out_bq; bp != NULL && bp->nadded >= accumulate &&
				bp->ndx == lastdone+nready*accumulate+1;
				bp = bp->next)
		++nready;

	return(nready);
}


/* Catch up with output queue by producing ready results */
static int
output_catchup(int nmax)
{
	int	nout = 0;
	BINQ	*bp;
	int	i;
					/* output ready results */
	while (out_bq != NULL && out_bq->nadded >= accumulate
				&& out_bq->ndx == lastdone+1) {
		if ((nmax > 0) & (nout >= nmax))
			break;
		bp = out_bq;			/* pop off first entry */
		out_bq = bp->next;
		bp->next = NULL;
		for (i = 0; i < nmods; i++)	/* output record */
			mod_output(bp->mca[i]);
		end_record();
		free_binq(bp);			/* free this entry */
		lastdone += accumulate;
		++nout;
	}
	return(nout);
}


/* Put a zero record in results queue & output */
void
put_zero_record(int ndx)
{
	BINQ	*bp = new_binq();
	int	i;

	for (i = nmods; i--; )
		memset(bp->mca[i]->cbin, 0, sizeof(DCOLOR)*bp->mca[i]->nbins);
	bp->ndx = ndx;
	bp->nadded = 1;
	queue_output(bp);
	output_catchup(0);
}


/* Get results from child process and add to queue */
static void
queue_results(int k)
{
	BINQ	*bq = new_binq();	/* get results holder */
	int	j;

	bq->ndx = kida[k].r1;
	bq->nadded = kida[k].nr;
					/* read from child */
	for (j = 0; j < nmods; j++)
		if (getbinary(bq->mca[j]->cbin, sizeof(DCOLOR), bq->mca[j]->nbins,
					kida[k].infp) != bq->mca[j]->nbins)
			error(SYSTEM, "read error from render process");
			
	queue_output(bq);		/* put results in output queue */
	kida[k].nr = 0;			/* mark child as available */
}


/* callback to set output spec to NULL (stdout) */
static int
set_stdout(const LUENT *le, void *p)
{
	(*(MODCONT *)le->data).outspec = NULL;
	return(0);
}


/* Start child processes if we can (call only once in parent!) */
int
in_rchild()
{
	int	rval;

	while (nchild < nproc) {	/* fork until target reached */
		errno = 0;
		rval = open_process(&kidpr[nchild], NULL);
		if (rval < 0)
			error(SYSTEM, "open_process() call failed");
		if (rval == 0) {	/* if in child, set up & return true */
			lu_doall(&modconttab, &set_stdout, NULL);
			lu_done(&ofiletab);
			while (nchild--) {	/* don't share other pipes */
				close(kidpr[nchild].w);
				fclose(kida[nchild].infp);
			}
			inpfmt = (sizeof(RREAL)==sizeof(double)) ? 'd' : 'f';
			outfmt = 'd';
			header = 0;
			yres = 0;
			raysleft = 0;
			if (accumulate == 1) {
				waitflush = xres = 1;
				account = accumulate = 1;
			} else {	/* parent controls accumulation */
				waitflush = xres = 0;
				account = accumulate = 0;
			}
			return(1);	/* return "true" in child */
		}
		if (rval != PIPE_BUF)
			error(CONSISTENCY, "bad value from open_process()");
					/* connect to child's output */
		kida[nchild].infp = fdopen(kidpr[nchild].r, "rb");
		if (kida[nchild].infp == NULL)
			error(SYSTEM, "out of memory in in_rchild()");
		kida[nchild++].nr = 0;	/* mark as available */
	}
#ifdef getc_unlocked
	for (rval = nchild; rval--; )	/* avoid mutex overhead */
		flockfile(kida[rval].infp);
#endif
	return(0);			/* return "false" in parent */
}


/* Close child processes */
void
end_children(int immed)
{
	int	i;

#ifdef SIGKILL				/* error mode -- quick exit */
	for (i = nchild*immed; i-- > 0; )
		kill(kidpr[nchild].pid, SIGKILL);
#endif
	if ((i = close_processes(kidpr, nchild)) > 0 && !immed) {
		sprintf(errmsg, "rendering process returned bad status (%d)",
					i);
			error(WARNING, errmsg);
	}
	while (nchild-- > 0)
		fclose(kida[nchild].infp);
}


/* Wait for the next available child, managing output queue simultaneously */
static int
next_child_nq(int flushing)
{
	static struct timeval	polling;
	struct timeval		*pmode;
	fd_set			readset, errset;
	int			i, n, nr, nqr;

	if (!flushing)			/* see if there's one free */
		for (i = nchild; i--; )
			if (!kida[i].nr)
				return(i);

	nqr = queue_ready();		/* choose blocking mode or polling */
	if ((nqr > 0) & !flushing)
		pmode = &polling;
	else
		pmode = NULL;
tryagain:				/* catch up with output? */
	if (pmode == &polling) {
		if (nqr > nchild)	/* don't get too far behind */
			nqr -= output_catchup(nqr-nchild);
	} else if (nqr > 0)		/* clear output before blocking */
		nqr -= output_catchup(0);
					/* prepare select() call */
	FD_ZERO(&readset); FD_ZERO(&errset);
	n = nr = 0;
	for (i = nchild; i--; ) {
		if (kida[i].nr) {
			FD_SET(kidpr[i].r, &readset);
			++nr;
		}
		FD_SET(kidpr[i].r, &errset);
		if (kidpr[i].r >= n)
			n = kidpr[i].r + 1;
	}
	if (!nr)			/* nothing to wait for? */
		return(-1);
	if ((nr > 1) | (pmode == &polling)) {
		errno = 0;
		nr = select(n, &readset, NULL, &errset, pmode);
		if (!nr) {
			pmode = NULL;	/* try again, blocking this time */
			goto tryagain;
		}
		if (nr < 0)
			error(SYSTEM, "select() error in next_child_nq()");
	} else
		FD_ZERO(&errset);
	n = -1;				/* read results from child(ren) */
	for (i = nchild; i--; ) {
		if (FD_ISSET(kidpr[i].r, &errset))
			error(USER, "rendering process died");
		if (FD_ISSET(kidpr[i].r, &readset))
			queue_results(n = i);
	}
	return(n);			/* first available child */
}


/* Run parental oversight loop */
void
parental_loop()
{
	const int	qlimit = (accumulate == 1) ? 1 : MAXIQ-1;
	int		ninq = 0;
	FVECT		orgdir[2*MAXIQ];
	int		i, n;
					/* load rays from stdin & process */
#ifdef getc_unlocked
	flockfile(stdin);		/* avoid lock/unlock overhead */
#endif
	while (getvec(orgdir[2*ninq]) == 0 && getvec(orgdir[2*ninq+1]) == 0) {
		const int	zero_ray = orgdir[2*ninq+1][0] == 0.0 &&
						(orgdir[2*ninq+1][1] == 0.0) &
						(orgdir[2*ninq+1][2] == 0.0);
		ninq += !zero_ray;
				/* Zero ray cannot go in input queue */
		if (zero_ray ? ninq : ninq >= qlimit ||
			    lastray/accumulate != (lastray+ninq)/accumulate) {
			i = next_child_nq(0);		/* manages output */
			n = ninq;
			if (accumulate > 1)		/* need terminator? */
				memset(orgdir[2*n++], 0, sizeof(FVECT)*2);
			n *= sizeof(FVECT)*2;		/* send assignment */
			if (writebuf(kidpr[i].w, (char *)orgdir, n) != n)
				error(SYSTEM, "pipe write error");
			kida[i].r1 = lastray+1;
			lastray += kida[i].nr = ninq;	/* mark as busy */
			if (lastray < lastdone) {	/* RNUMBER wrapped? */
				while (next_child_nq(1) >= 0)
					;
				lastray -= ninq;
				lastdone = lastray %= accumulate;
			}
			ninq = 0;
		}
		if (zero_ray) {				/* put bogus record? */
			if ((yres <= 0) | (xres <= 1) &&
					(lastray+1) % accumulate == 0) {
				while (next_child_nq(1) >= 0)
					;		/* clear the queue */
				lastdone = lastray = accumulate-1;
				waitflush = 1;		/* flush next */
			}
			put_zero_record(++lastray);
		}
		if (!morays())
			break;				/* preemptive EOI */
	}
	while (next_child_nq(1) >= 0)		/* empty results queue */
		;
	if (account < accumulate) {
		error(WARNING, "partial accumulation in final record");
		free_binq(out_bq);		/* XXX just ignore it */
		out_bq = NULL;
	}
	free_binq(NULL);			/* clean up */
	lu_done(&ofiletab);
	if (raysleft)
		error(USER, "unexpected EOF on input");
}


/* Wait for the next available child by monitoring "to" pipes */
static int
next_child_ready()
{
	fd_set			writeset, errset;
	int			i, n;

	for (i = nchild; i--; )		/* see if there's one free first */
		if (!kida[i].nr)
			return(i);
					/* prepare select() call */
	FD_ZERO(&writeset); FD_ZERO(&errset);
	n = 0;
	for (i = nchild; i--; ) {
		FD_SET(kidpr[i].w, &writeset);
		FD_SET(kidpr[i].r, &errset);
		if (kidpr[i].w >= n)
			n = kidpr[i].w + 1;
		if (kidpr[i].r >= n)
			n = kidpr[i].r + 1;
	}
	errno = 0;
	n = select(n, NULL, &writeset, &errset, NULL);
	if (n < 0)
		error(SYSTEM, "select() error in next_child_ready()");
	n = -1;				/* identify waiting child */
	for (i = nchild; i--; ) {
		if (FD_ISSET(kidpr[i].r, &errset))
			error(USER, "rendering process died");
		if (FD_ISSET(kidpr[i].w, &writeset))
			kida[n = i].nr = 0;
	}
	return(n);			/* first available child */
}


/* Modified parental loop for full accumulation mode (-c 0) */
void
feeder_loop()
{
	static int	ignore_warning_given = 0;
	int		ninq = 0;
	FVECT		orgdir[2*MAXIQ];
	int		i, n;
					/* load rays from stdin & process */
#ifdef getc_unlocked
	flockfile(stdin);		/* avoid lock/unlock overhead */
#endif
	while (getvec(orgdir[2*ninq]) == 0 && getvec(orgdir[2*ninq+1]) == 0) {
		if (orgdir[2*ninq+1][0] == 0.0 &&	/* asking for flush? */
				(orgdir[2*ninq+1][1] == 0.0) &
				(orgdir[2*ninq+1][2] == 0.0)) {
			if (!ignore_warning_given++)
				error(WARNING,
				"dummy ray(s) ignored during accumulation\n");
			continue;
		} 
		if (++ninq >= MAXIQ) {
			i = next_child_ready();		/* get eager child */
			n = sizeof(FVECT)*2 * ninq;	/* give assignment */
			if (writebuf(kidpr[i].w, (char *)orgdir, n) != n)
				error(SYSTEM, "pipe write error");
			kida[i].r1 = lastray+1;
			lastray += kida[i].nr = ninq;
			if (lastray < lastdone)		/* RNUMBER wrapped? */
				lastdone = lastray = 0;
			ninq = 0;
		}
		if (!morays())
			break;				/* preemptive EOI */
	}
	if (ninq) {				/* polish off input */
		i = next_child_ready();
		n = sizeof(FVECT)*2 * ninq;
		if (writebuf(kidpr[i].w, (char *)orgdir, n) != n)
			error(SYSTEM, "pipe write error");
		kida[i].r1 = lastray+1;
		lastray += kida[i].nr = ninq;
		ninq = 0;
	}
	memset(orgdir, 0, sizeof(FVECT)*2);	/* get results */
	for (i = nchild; i--; ) {
		writebuf(kidpr[i].w, (char *)orgdir, sizeof(FVECT)*2);
		queue_results(i);
	}
	if (recover)				/* and from before? */
		queue_modifiers();
	end_children(0);			/* free up file descriptors */
	for (i = 0; i < nmods; i++)
		mod_output(out_bq->mca[i]);	/* output accumulated record */
	end_record();
	free_binq(out_bq);			/* clean up */
	out_bq = NULL;
	free_binq(NULL);
	lu_done(&ofiletab);
	if (raysleft)
		error(USER, "unexpected EOF on input");
}
