/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Optimize holodeck for quick access.
 *
 *	11/4/98		Greg Ward Larson
 */

#include "holo.h"

#include <signal.h>

#ifndef BKBSIZE
#define BKBSIZE		256		/* beam clump size (kilobytes) */
#endif

#define flgop(p,i,op)		((p)[(i)>>5] op (1L<<((i)&0x1f)))
#define isset(p,i)		flgop(p,i,&)
#define setfl(p,i)		flgop(p,i,|=)
#define clrfl(p,i)		flgop(p,i,&=~)

char	*progname;
char	tempfile[128];

extern char	*rindex();
extern int	quit();
extern long	rhinitcopy();


main(argc, argv)
int	argc;
char	*argv[];
{
	char	*inpname, *outname;
	int	hdfd[2];
	long	nextipos, lastopos, thisopos;

	progname = argv[0];
	if (argc < 2 | argc > 3) {
		fprintf(stderr, "Usage: %s input.hdk [output.hdk]\n", progname);
		exit(1);
	}
	inpname = argv[1];
	if (argc == 3)			/* use given output file */
		outname = argv[2];
	else {				/* else use temporary file */
		if (access(inpname, R_OK|W_OK) < 0) {	/* check permissions */
			sprintf(errmsg, "cannot access \"%s\"", inpname);
			error(SYSTEM, errmsg);
		}
		strcpy(tempfile, inpname);
		if ((outname = rindex(tempfile, '/')) != NULL)
			outname++;
		else
			outname = tempfile;
		sprintf(outname, "rho%d.hdk", getpid());
		outname = tempfile;
	}
					/* copy holodeck file header */
	nextipos = rhinitcopy(hdfd, inpname, outname);
	lastopos = 0L;			/* copy sections one by one */
	while (nextipos != 0L) {
					/* set input position; get next */
		lseek(hdfd[0], nextipos, 0);
		read(hdfd[0], (char *)&nextipos, sizeof(nextipos));
					/* get output position; set last */
		thisopos = lseek(hdfd[1], 0L, 2);
		if (lastopos > 0L) {
			lseek(hdfd[1], lastopos, 0);
			write(hdfd[1], (char *)&thisopos, sizeof(thisopos));
			lseek(hdfd[1], 0L, 2);
		}
		lastopos = thisopos;
		thisopos = 0L;		/* write place holder */
		write(hdfd[1], (char *)&thisopos, sizeof(thisopos));
					/* copy holodeck section */
		copysect(hdfd[0], hdfd[1]);
	}
					/* clean up */
	close(hdfd[0]);
	close(hdfd[1]);
	if (outname == tempfile && rename(outname, inpname) < 0) {
		sprintf(errmsg, "cannot rename \"%s\" to \"%s\"",
				outname, inpname);
		error(SYSTEM, errmsg);
	}
	exit(0);
}


long
rhinitcopy(hfd, infn, outfn)	/* open files and copy header */
int	hfd[2];			/* returned file descriptors */
char	*infn, *outfn;
{
	FILE	*infp, *outfp;
	long	ifpos;
					/* open files for i/o */
	if ((infp = fopen(infn, "r")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\" for reading", infn);
		error(SYSTEM, errmsg);
	}
	if ((outfp = fopen(outfn, "w+")) == NULL) {
		sprintf(errmsg, "cannot open \"%s\" for writing", outfn);
		error(SYSTEM, errmsg);
	}
					/* set up signal handling */
	if (signal(SIGINT, quit) == SIG_IGN) signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, quit) == SIG_IGN) signal(SIGHUP, SIG_IGN);
	if (signal(SIGTERM, quit) == SIG_IGN) signal(SIGTERM, SIG_IGN);
#ifdef SIGXCPU
	if (signal(SIGXCPU, quit) == SIG_IGN) signal(SIGXCPU, SIG_IGN);
	if (signal(SIGXFSZ, quit) == SIG_IGN) signal(SIGXFSZ, SIG_IGN);
#endif
					/* copy and verify header */
	if (checkheader(infp, HOLOFMT, outfp) < 0 || getw(infp) != HOLOMAGIC)
		error(USER, "input not in holodeck format");
	fputformat(HOLOFMT, outfp);
	fputc('\n', outfp);
	putw(HOLOMAGIC, outfp);
					/* get descriptors and free stdio */
	if ((hfd[0] = dup(fileno(infp))) < 0 ||
			(hfd[1] = dup(fileno(outfp))) < 0)
		error(SYSTEM, "dup call failed in rhinitcopy");
	ifpos = ftell(infp);
	fclose(infp);
	if (fclose(outfp) == EOF)
		error(SYSTEM, "file flushing error in rhinitcopy");
					/* flush everything manually hence */
	hdcachesize = 0;
					/* return input position */
	return(ifpos);
}


gcshifti(gc, ia, di, hp)	/* shift cell row or column */
register GCOORD	*gc;
int	ia, di;
register HOLO	*hp;
{
	int	nw;

	if (di > 0) {
		if (++gc->i[ia] >= hp->grid[((gc->w>>1)+1+ia)%3]) {
			nw = ((gc->w&~1) + (ia<<1) + 3) % 6;
			gc->i[ia] = gc->i[1-ia];
			gc->i[1-ia] = gc->w&1 ? hp->grid[((nw>>1)+2-ia)%3]-1 : 0;
			gc->w = nw;
		}
	} else if (di < 0) {
		if (--gc->i[ia] < 0) {
			nw = ((gc->w&~1) + (ia<<1) + 2) % 6;
			gc->i[ia] = gc->i[1-ia];
			gc->i[1-ia] = gc->w&1 ? hp->grid[((nw>>1)+2-ia)%3]-1 : 0;
			gc->w = nw;
		}
	}
}


mkneighgrid(ng, hp, gc)		/* compute neighborhood for grid cell */
GCOORD	ng[3*3];
HOLO	*hp;
GCOORD	*gc;
{
	GCOORD	gci0;
	register int	i, j;

	for (i = 3; i--; ) {
		copystruct(&gci0, gc);
		gcshifti(&gci0, 0, i-1, hp);
		for (j = 3; j--; ) {
			copystruct(ng+(3*i+j), &gci0);
			gcshifti(ng+(3*i+j), gci0.w==gc->w, j-1, hp);
		}
	}
}


int	bneighlist[9*9-1];
int	bneighrem;

#define nextneigh()	(bneighrem<=0 ? 0 : bneighlist[--bneighrem])

int
firstneigh(hp, b)		/* initialize neighbor list and return first */
HOLO	*hp;
int	b;
{
	GCOORD	wg0[9], wg1[9], bgc[2];
	int	i, j;

	hdbcoord(bgc, hp, b);
	mkneighgrid(wg0, hp, bgc);
	mkneighgrid(wg1, hp, bgc+1);
	bneighrem = 0;
	for (i = 9; i--; )
		for (j = 9; j--; ) {
			if (i == 4 & j == 4)	/* don't copy starting beam */
				continue;
			if (wg0[i].w == wg1[j].w)
				continue;
			copystruct(bgc, wg0+i);
			copystruct(bgc+1, wg1+j);
			bneighlist[bneighrem++] = hdbindex(hp, bgc);
#ifdef DEBUG
			if (bneighlist[bneighrem-1] <= 0)
				error(CONSISTENCY, "bad beam in firstneigh");
#endif
		}
	return(nextneigh());
}


BEAMI	*beamdir;

int
bpcmp(b1p, b2p)			/* compare beam positions on disk */
int	*b1p, *b2p;
{
	register long	pdif = beamdir[*b1p].fo - beamdir[*b2p].fo;

	if (pdif > 0L) return(1);
	if (pdif < 0L) return(-1);
	return(0);
}


copysect(ifd, ofd)		/* copy holodeck section from ifd to ofd */
int	ifd, ofd;
{
	static short	primes[] = {9431,6803,4177,2659,1609,887,587,251,47,1};
	register HOLO	*hinp;
	HOLO	*hout;
	register BEAM	*bp;
	unsigned int4	*bflags;
	int	*bqueue;
	int	bqlen;
	int4	bqtotal;
	int	bc, bci, bqc, myprime;
	register int	i;
					/* load input section directory */
	hinp = hdinit(ifd, NULL);
					/* create output section directory */
	hout = hdinit(ofd, (HDGRID *)hinp);
					/* allocate beam queue */
	bqueue = (int *)malloc(nbeams(hinp)*sizeof(int));
	bflags = (unsigned int4 *)calloc((nbeams(hinp)>>5)+1,
			sizeof(unsigned int4));
	if (bqueue == NULL | bflags == NULL)
		error(SYSTEM, "out of memory in copysect");
					/* mark empty beams as done */
	for (i = nbeams(hinp); i > 0; i--)
		if (!hinp->bi[i].nrd)
			setfl(bflags, i);
					/* pick a good prime step size */
	for (i = 0; primes[i]<<5 >= nbeams(hinp); i++)
		;
	while ((myprime = primes[i++]) > 1)
		if (nbeams(hinp) % myprime)
			break;
					/* add each input beam and neighbors */
	for (bc = bci = nbeams(hinp); bc > 0; bc--,
			bci += bci>myprime ? -myprime : nbeams(hinp)-myprime) {
		if (isset(bflags, bci))
			continue;
		bqueue[0] = bci;		/* initialize queue */
		bqlen = 1;
		bqtotal = hinp->bi[bci].nrd;
		setfl(bflags, bci);
						/* run through growing queue */
		for (bqc = 0; bqc < bqlen; bqc++) {
						/* add neighbors until full */
			for (i = firstneigh(hinp,bqueue[bqc]); i > 0;
					i = nextneigh()) {
				if (isset(bflags, i))	/* done already? */
					continue;
				bqueue[bqlen++] = i;	/* add it */
				bqtotal += hinp->bi[i].nrd;
				setfl(bflags, i);
				if (bqtotal >= BKBSIZE*1024/sizeof(RAYVAL))
					break;		/* queue full */
			}
			if (i > 0)
				break;
		}
		beamdir = hinp->bi;		/* sort queue */
		qsort((char *)bqueue, bqlen, sizeof(*bqueue), bpcmp);
						/* transfer each beam */
		for (i = 0; i < bqlen; i++) {
			bp = hdgetbeam(hinp, bqueue[i]);
			bcopy((char *)hdbray(bp),
				(char *)hdnewrays(hout,bqueue[i],bp->nrm),
					bp->nrm*sizeof(RAYVAL));
			hdfreebeam(hinp, bqueue[i]);
		}
		hdfreebeam(hout, 0);		/* flush output block */
#ifdef DEBUG
		hdsync(hout, 0);
#endif
	}
					/* we're done -- clean up */
	free((char *)bqueue);
	free((char *)bflags);
	hddone(hinp);
	hddone(hout);
}


eputs(s)			/* put error message to stderr */
register char  *s;
{
	static int  midline = 0;

	if (!*s)
		return;
	if (!midline++) {	/* prepend line with program name */
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (s[strlen(s)-1] == '\n') {
		fflush(stderr);
		midline = 0;
	}
}


quit(code)			/* exit the program gracefully */
int	code;
{
	if (tempfile[0])
		unlink(tempfile);
	exit(code);
}
