/* Copyright (c) 1989 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * colortab.c - allocate and control dynamic color table.
 *
 *	We start off with a uniform partition of color space.
 *	As pixels are sent to the frame buffer, a histogram is built.
 *	When a new color table is requested, the histogram is used
 *	to make a pseudo-optimal partition, after which the
 *	histogram is cleared.  This algorithm
 *	performs only as well as the next drawing's color
 *	distribution is correlated to the last.
 */

#include "color.h"

#define NULL		0
				/* histogram resolution */
#define NRED		24
#define NGRN		32
#define NBLU		18
#define HMAX		NGRN
				/* minimum box count for adaptive partition */
#define MINSAMP		7
				/* maximum frame buffer depth */
#define FBDEPTH		8
				/* color map resolution */
#define MAPSIZ		128
				/* map a color */
#define map_col(c,p)	clrmap[p][ colval(c,p)<1. ? \
				(int)(colval(c,p)*MAPSIZ) : MAPSIZ-1 ]
				/* color partition tree */
#define CNODE		short
#define set_branch(p,c)	((c)<<2|(p))
#define set_pval(pv)	((pv)<<2|3)
#define is_pval(cn)	(((cn)&3)==3)
#define part(cn)	((cn)>>2)
#define prim(cn)	((cn)&3)
#define pval(cn)	((cn)>>2)
				/* our color table */
COLR	clrtab[1<<FBDEPTH];
				/* our color correction map */
static BYTE	clrmap[3][MAPSIZ];
				/* histogram of colors used */
static unsigned	histo[NRED][NGRN][NBLU];
				/* initial color cube boundaries */
static int	CLRCUBE[3][2] = {0,NRED,0,NGRN,0,NBLU};
				/* color cube partition */
static CNODE	ctree[1<<(FBDEPTH+1)];


COLR *
get_ctab(ncolors)	/* assign a color table with max ncolors */
int	ncolors;
{
	if (ncolors < 1 || ncolors > 1<<FBDEPTH)
		return(NULL);
				/* partition color table */
	cut(ctree, FBDEPTH, CLRCUBE, 0, ncolors);
				/* clear histogram */
	bzero(histo, sizeof(histo));
				/* return color table */
	return(clrtab);
}


int
get_pixel(col)			/* get pixel for color */
COLOR	col;
{
	int	cv[3];
	register CNODE	*tp;
	register int	h;
					/* map color */
	cv[RED] = map_col(col,RED);
	cv[GRN] = map_col(col,GRN);
	cv[BLU] = map_col(col,BLU);
					/* add to histogram */
	histo[cv[RED]][cv[GRN]][cv[BLU]]++;
					/* find pixel value in tree */
	tp = ctree;
	for (h = FBDEPTH; h > 0; h--) {
		if (is_pval(*tp))
			break;
		if (cv[prim(*tp)] < part(*tp))
			tp++;			/* left branch */
		else
			tp += 1<<h;		/* right branch */
	}
#ifdef notdef
	printf("distance (%d,%d,%d)\n",
		cv[RED] - clrtab[pval(*tp)][RED]*NRED/256, 
		cv[GRN] - clrtab[pval(*tp)][GRN]*NGRN/256,
		cv[BLU] - clrtab[pval(*tp)][BLU]*NBLU/256);
#endif
	return(pval(*tp));
}


make_cmap(gam)			/* make gamma correction map */
double  gam;
{
	extern double	pow();
	double	d;
	register int	i;
	
	for (i = 0; i < MAPSIZ; i++) {
		d = pow(i/(double)MAPSIZ, 1.0/gam);
		clrmap[RED][i] = d * NRED;
		clrmap[GRN][i] = d * NGRN;
		clrmap[BLU][i] = d * NBLU;
	}
}


static
cut(tree, height, box, c0, c1)		/* partition color space */
register CNODE	*tree;
int	height;
register int	box[3][2];
int	c0, c1;
{
	int	kb[3][2];
	
	if (c1-c0 == 1) {		/* assign color */
		clrtab[c0][RED] = ((box[RED][0]+box[RED][1])<<7)/NRED;
		clrtab[c0][GRN] = ((box[GRN][0]+box[GRN][1])<<7)/NGRN;
		clrtab[c0][BLU] = ((box[BLU][0]+box[BLU][1])<<7)/NBLU;
		clrtab[c0][EXP] = COLXS;
		*tree = set_pval(c0);
#ifdef notdef
		printf("final box size = (%d,%d,%d)\n",
			box[RED][1] - box[RED][0],
			box[GRN][1] - box[GRN][0],
			box[BLU][1] - box[BLU][0]);
#endif
		return;
	}
					/* split box */
	*tree = split(box);
	bcopy(box, kb, sizeof(kb));
	kb[prim(*tree)][1] = part(*tree);
	cut(tree+1, height-1, kb, c0, (c0+c1)>>1);		/* lesser */
	kb[prim(*tree)][0] = part(*tree);
	kb[prim(*tree)][1] = box[prim(*tree)][1];
	cut(tree+(1<<height), height-1, kb, (c0+c1)>>1, c1);	/* greater */
}


static int
split(box)				/* find median cut for box */
register int	box[3][2];
{
#define c0	r
	register int	r, g, b;
	int	pri;
	int	t[HMAX], med;
					/* find dominant axis */
	pri = RED;
	if (box[GRN][1]-box[GRN][0] > box[pri][1]-box[pri][0])
		pri = GRN;
	if (box[BLU][1]-box[BLU][0] > box[pri][1]-box[pri][0])
		pri = BLU;
					/* sum histogram over box */
	med = 0;
	switch (pri) {
	case RED:
		for (r = box[RED][0]; r < box[RED][1]; r++) {
			t[r] = 0;
			for (g = box[GRN][0]; g < box[GRN][1]; g++)
				for (b = box[BLU][0]; b < box[BLU][1]; b++)
					t[r] += histo[r][g][b];
			med += t[r];
		}
		break;
	case GRN:
		for (g = box[GRN][0]; g < box[GRN][1]; g++) {
			t[g] = 0;
			for (b = box[BLU][0]; b < box[BLU][1]; b++)
				for (r = box[RED][0]; r < box[RED][1]; r++)
					t[g] += histo[r][g][b];
			med += t[g];
		}
		break;
	case BLU:
		for (b = box[BLU][0]; b < box[BLU][1]; b++) {
			t[b] = 0;
			for (r = box[RED][0]; r < box[RED][1]; r++)
				for (g = box[GRN][0]; g < box[GRN][1]; g++)
					t[b] += histo[r][g][b];
			med += t[b];
		}
		break;
	}
	if (med < MINSAMP)		/* if too sparse, split at midpoint */
		return(set_branch(pri,(box[pri][0]+box[pri][1])>>1));
					/* find median position */
	med >>= 1;
	for (c0 = box[pri][0]; med > 0; c0++)
		med -= t[c0];
	if (c0 > (box[pri][0]+box[pri][1])>>1)	/* if past the midpoint */
		c0--;				/* part left of median */
	return(set_branch(pri,c0));
#undef c0
}
