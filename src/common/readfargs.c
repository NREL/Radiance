/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Allocate, read and free object arguments
 */

#include <stdio.h>

#include "fvect.h"

#include "object.h"


extern char  *savestr(), *malloc(), *fgetword();
extern double  atof();
extern int  atoi();
extern long  atol();

#ifdef  MEMHOG
extern char  *bmalloc();
#else
#define  bmalloc	malloc
#endif


readfargs(fa, fp)		/* read function arguments from stream */
register FUNARGS  *fa;
FILE  *fp;
{
#define getstr()	(fgetword(sbuf,MAXSTR,fp)!=NULL)
#define getint()	(getstr() && isint(sbuf))
#define getflt()	(getstr() && isflt(sbuf))
	char  sbuf[MAXSTR];
	register int  n, i;

	if (!getint() || (n = atoi(sbuf)) < 0)
		return(0);
	if (fa->nsargs = n) {
		fa->sarg = (char **)bmalloc(n*sizeof(char *));
		if (fa->sarg == NULL)
			return(-1);
		for (i = 0; i < fa->nsargs; i++) {
			if (!getstr())
				return(0);
			fa->sarg[i] = savestr(sbuf);
		}
	} else
		fa->sarg = NULL;
	if (!getint() || (n = atoi(sbuf)) < 0)
		return(0);
#ifdef  IARGS
	if (fa->niargs = n) {
		fa->iarg = (long *)bmalloc(n*sizeof(long));
		if (fa->iarg == NULL)
			return(-1);
		for (i = 0; i < n; i++) {
			if (!getint())
				return(0);
			fa->iarg[i] = atol(sbuf);
		}
	} else
		fa->iarg = NULL;
#else
	if (n != 0)
		return(0);
#endif
	if (!getint() || (n = atoi(sbuf)) < 0)
		return(0);
	if (fa->nfargs = n) {
		fa->farg = (FLOAT *)bmalloc(n*sizeof(FLOAT));
		if (fa->farg == NULL)
			return(-1);
		for (i = 0; i < n; i++) {
			if (!getflt())
				return(0);
			fa->farg[i] = atof(sbuf);
		}
	} else
		fa->farg = NULL;
	return(1);
#undef getflt
#undef getint
#undef getstr
}


#ifndef  MEMHOG
freefargs(fa)			/* free object arguments */
register FUNARGS  *fa;
{
	register int  i;

	if (fa->nsargs) {
		for (i = 0; i < fa->nsargs; i++)
			freestr(fa->sarg[i]);
		free((char *)fa->sarg);
	}
#ifdef  IARGS
	if (fa->niargs)
		free((char *)fa->iarg);
#endif
	if (fa->nfargs)
		free((char *)fa->farg);
}
#endif
