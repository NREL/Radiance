/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Read and write portable ambient values
 */

#include <stdio.h>

#include "color.h"

#include "ambient.h"


#define  putvec(v,fp)	putflt((v)[0],fp);putflt((v)[1],fp);putflt((v)[2],fp)

#define  getvec(v,fp)	(v)[0]=getflt(fp);(v)[1]=getflt(fp);(v)[2]=getflt(fp)


extern double  getflt();
extern long  getint();


writambval(av, fp)		/* write ambient value to stream */
register AMBVAL  *av;
FILE  *fp;
{
	COLR  col;

	putint((long)av->lvl, 1, fp);
	putflt(av->weight, fp);
	putvec(av->pos, fp);
	putvec(av->dir, fp);
	setcolr(col, colval(av->val,RED),
			colval(av->val,GRN), colval(av->val,BLU));
	fwrite((char *)col, sizeof(col), 1, fp);
	putflt(av->rad, fp);
	putvec(av->gpos, fp);
	putvec(av->gdir, fp);
	return(ferror(fp) ? -1 : 0);
}


readambval(av, fp)		/* read ambient value from stream */
register AMBVAL  *av;
FILE  *fp;
{
	COLR  col;

	av->lvl = getint(1, fp);
	if (feof(fp))
		return(0);
	av->weight = getflt(fp);
	getvec(av->pos, fp);
	getvec(av->dir, fp);
	if (fread((char *)col, sizeof(col), 1, fp) != 1)
		return(0);
	colr_color(av->val, col);
	av->rad = getflt(fp);
	getvec(av->gpos, fp);
	getvec(av->gdir, fp);
	return(feof(fp) ? 0 : 1);
}
