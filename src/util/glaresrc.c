/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Gather samples and compute glare sources.
 */

#include "glare.h"

#define vcont(vd,vu)	((vu)-(vd)<=SEPS)
#define hcont(s1,s2)	((s1)->r-(s2)->l>=-SEPS&&(s2)->r-(s1)->l>=-SEPS)

struct source	*curlist = NULL;	/* current source list */
struct source	*donelist = NULL;	/* finished sources */


struct srcspan *
newspan(l, r, v, sb)		/* allocate a new source span */
int	l, r, v;
float	*sb;
{
	register struct srcspan	*ss;
	register int	i;

	ss = (struct srcspan *)malloc(sizeof(struct srcspan));
	if (ss == NULL)
		memerr("source spans");
	ss->l = l;
	ss->r = r;
	ss->v = v;
	ss->brsum = 0.0;
	for (i = l; i < r; i++)
		ss->brsum += sb[i+hsize];
	return(ss);
}


analyze()			/* analyze our scene */
{
	int	h, v;
	int	left;
	float	*spanbr;

	spanbr = (float *)malloc((2*hsize+1)*sizeof(float));
	if (spanbr == NULL)
		memerr("view span brightness buffer");
	for (v = vsize; v >= -vsize; v--) {
		close_sources(v);
#ifndef DEBUG
		if (verbose)
			fprintf(stderr, "%s: analyzing... %3ld%%\r",
				progname, 100L*(vsize-v)/(2*vsize));
#endif
		getviewspan(v, spanbr);
		left = hsize + 1;
		for (h = -hsize; h <= hsize; h++) {
			if (spanbr[h+hsize] < 0.0) {	/* off view */
				if (left < h) {
					addsrcspan(newspan(left,h,v,spanbr));
					left = hsize + 1;
				}
				continue;
			}
			if (spanbr[h+hsize] > threshold) {	/* in source */
				if (left > h)
					left = h;
			} else {			/* out of source */
				if (left < h) {
					addsrcspan(newspan(left,h,v,spanbr));
					left = hsize + 1;
				}
				addindirect(h, spanbr[h+hsize]);
			}
		}
		if (left < h)
			addsrcspan(newspan(left,h,v,spanbr));
	}
	free((char *)spanbr);
	close_allsrcs();
}


addindirect(h, br)		/* add brightness to indirect illuminances */
int	h;
double	br;
{
	double	tanb, d;
	register int	i;

	if (h <= -hlim) {			/* left region */
		d = (double)(-h-hlim)/sampdens;
		if (d >= 1.0-FTINY)
			return;
		tanb = d/sqrt(1.0-d*d);
		for (i = 0; i < nglardirs; i++) {
			d = indirect[i].lcos - tanb*indirect[i].lsin;
			if (d > 0.0) {
				indirect[i].sum += d * br;
				indirect[i].n += d;
			}
		}
		return;
	}
	if (h >= hlim) {			/* right region */
		d = (double)(-h+hlim)/sampdens;
		if (d <= -1.0+FTINY)
			return;
		tanb = d/sqrt(1.0-d*d);
		for (i = 0; i < nglardirs; i++) {
			d = indirect[i].rcos - tanb*indirect[i].rsin;
			if (d > 0.0) {
				indirect[i].sum += d * br;
				indirect[i].n += d;
			}
		}
		return;
	}
					/* central region */
	for (i = 0; i < nglardirs; i++) {
		d = cos(h_theta(h) - indirect[i].theta);
		if (d > 0.0) {
			indirect[i].sum += d * br;
			indirect[i].n += d;
		}
	}
}


comp_thresh()			/* compute glare threshold */
{
	SPANERR	sperr;
	int	h, v;
	int	nsamps;
	double	brsum, br;

	if (verbose)
		fprintf(stderr, "%s: computing glare threshold...\n",
				progname);
	brsum = 0.0;
	nsamps = 0;
	for (v = vsize; v >= -vsize; v -= TSAMPSTEP) {
		setspanerr(&sperr, v);
		for (h = -hsize; h <= hsize; h += TSAMPSTEP) {
			if ((br = getviewpix(h, v, &sperr)) < 0.0)
				continue;
			brsum += br;
			nsamps++;
		}
	}
	if (nsamps == 0) {
		fprintf(stderr, "%s: no viewable scene!\n", progname);
		exit(1);
	}
	threshold = GLAREBR * brsum / nsamps;
	if (threshold <= FTINY) {
		fprintf(stderr, "%s: threshold zero!\n", progname);
		exit(1);
	}
	if (verbose) {
#ifdef DEBUG
		pict_stats();
#endif
		fprintf(stderr,
			"%s: threshold set to %f cd/m2 from %d samples\n",
				progname, threshold, nsamps);
	}
}


addsrcspan(nss)			/* add new source span to our list */
struct srcspan	*nss;
{
	struct source	*last, *cs, *this;
	register struct srcspan	*ss;

	cs = NULL;
	for (this = curlist; this != NULL; this = this->next) {
		for (ss = this->first; ss != NULL; ss = ss->next) {
			if (!vcont(nss->v, ss->v))
				break;
			if (hcont(ss, nss)) {
				if (cs == NULL)
					cs = this;
				else {
					last->next = this->next;
					mergesource(cs, this);
					this = last;
				}
				break;
			}
		}
		last = this;
	}
	if (cs == NULL) {
		cs = (struct source *)malloc(sizeof(struct source));
		if (cs == NULL)
			memerr("source records");
		cs->dom = 0.0;
		cs->first = NULL;
		cs->next = curlist;
		curlist = cs;
	}
	nss->next = cs->first;
	cs->first = nss;
}


mergesource(sp, ap)		/* merge source ap into source sp */
struct source	*sp, *ap;
{
	struct srcspan	head;
	register struct srcspan	*alp, *prev, *tp;

	head.next = sp->first;
	prev = &head;
	alp = ap->first;
	while (alp != NULL && prev->next != NULL) {
		if (prev->next->v > alp->v) {
			tp = alp->next;
			alp->next = prev->next;
			prev->next = alp;
			alp = tp;
		}
		prev = prev->next;
	}
	if (prev->next == NULL)
		prev->next = alp;
	sp->first = head.next;
	if (ap->dom > 0.0 && sp->dom > 0.0) {		/* sources are done */
		sp->dir[0] *= sp->dom;
		sp->dir[1] *= sp->dom;
		sp->dir[2] *= sp->dom;
		fvsum(sp->dir, sp->dir, ap->dir, ap->dom);
		normalize(sp->dir);
		sp->brt = (sp->brt*sp->dom + ap->brt*ap->dom)
				/ (sp->dom + ap->dom);
	}
	free((char *)ap);
}


close_sources(v)		/* close sources above v */
int	v;
{
	struct source	head;
	register struct source	*last, *this;

	head.next = curlist;
	last = &head;
	for (this = curlist; this != NULL; this = this->next)
		if (!vcont(v, this->first->v)) {
			last->next = this->next;
			donesource(this);
			this = last;
		} else
			last = this;
	curlist = head.next;
}


close_allsrcs()			/* done with everything */
{
	register struct source	*this, *next;

	this = curlist;
	while (this != NULL) {
		next = this->next;
		donesource(this);
		this = next;
	}
	curlist = NULL;
}


donesource(sp)			/* finished with this source */
register struct source	*sp;
{
	FVECT	dthis, dright;
	register struct srcspan	*ss;
	int	h, n;
	double	d;

	sp->dom = 0.0;
	sp->dir[0] = sp->dir[1] = sp->dir[2] = 0.0;
	sp->brt = 0.0;
	n = 0;
	for (ss = sp->first; ss != NULL; ss = ss->next) {
		sp->brt += ss->brsum;
		n += ss->r - ss->l;
		if (compdir(dright, ss->r, ss->v, NULL) < 0)
			compdir(dright, ss->r-2, ss->v, NULL);
		for (h = ss->r-1; h >= ss->l; h--)
			if (compdir(dthis, h, ss->v, NULL) == 0) {
				d = dist2(dthis, dright);
				fvsum(sp->dir, sp->dir, dthis, d);
				sp->dom += d;
				VCOPY(dright, dthis);
			}
		free((char *)ss);
	}
	sp->first = NULL;
	sp->brt /= (double)n;
	normalize(sp->dir);
	sp->next = donelist;
	donelist = sp;
	if (verbose)
		fprintf(stderr,
	"%s: source at [%.3f,%.3f,%.3f], dw %.5f, br %.1f (%d samps)\n",
			progname, sp->dir[0], sp->dir[1], sp->dir[2], 
			sp->dom, sp->brt, n);
}


struct source *
findbuddy(s, l)			/* find close enough source to s in l*/
register struct source	*s, *l;
{
	struct source	*bestbuddy = NULL;
	double	d, r, mindist = MAXBUDDY;

	r = sqrt(s->dom/PI);
	for ( ; l != NULL; l = l->next) {
		d = sqrt(dist2(l->dir, s->dir)) - sqrt(l->dom/PI) - r;
		if (d < mindist) {
			bestbuddy = l;
			mindist = d;
		}
	}
	return(bestbuddy);
}


absorb_specks()			/* eliminate too-small sources */
{
	struct source	head, *buddy;
	register struct source	*last, *this;

	if (verbose)
		fprintf(stderr, "%s: absorbing small sources...\n", progname);
	head.next = donelist;
	last = &head;
	for (this = head.next; this != NULL; this = this->next)
		if (TOOSMALL(this)) {
			last->next = this->next;
			buddy = findbuddy(this, head.next);
			if (buddy != NULL)
				mergesource(buddy, this);
			else
				absorb(this);
			this = last;
		} else
			last = this;
	donelist = head.next;
}


absorb(s)			/* absorb a source into indirect */
register struct source	*s;
{
	FVECT	dir;
	double	d;
	register int	i;

	for (i = 0; i < nglardirs; i++) {
		spinvector(dir, ourview.vdir, ourview.vup, indirect[i].theta);
		d = DOT(dir,s->dir)*s->dom*(sampdens*sampdens);
		if (d <= 0.0)
			continue;
		indirect[i].sum += d * s->brt;
		indirect[i].n += d;
	}
	for ( ; s->first != NULL; s->first = s->first->next)
		free((char *)s->first);
	free((char *)s);
}
