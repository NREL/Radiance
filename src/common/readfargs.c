/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Allocate, read and free object arguments
 */

#include <stdio.h>

#include "object.h"


extern char  *savestr(), *malloc();

#ifdef  MEMHOG
extern char  *bmalloc();
#else
#define  bmalloc	malloc
#endif


readfargs(fa, fp)		/* read function arguments from stream */
register FUNARGS  *fa;
FILE  *fp;
{
	char  sbuf[MAXSTR];
	int  n;
	register int  i;

	if (fscanf(fp, "%d", &n) != 1 || n < 0)
		return(0);
	if (fa->nsargs = n) {
		fa->sarg = (char **)bmalloc(n*sizeof(char *));
		if (fa->sarg == NULL)
			return(-1);
		for (i = 0; i < fa->nsargs; i++) {
			if (fscanf(fp, "%s", sbuf) != 1)
				return(0);
			fa->sarg[i] = savestr(sbuf);
		}
	} else
		fa->sarg = NULL;
	if (fscanf(fp, "%d", &n) != 1 || n < 0)
		return(0);
#ifdef  IARGS
	if (fa->niargs = n) {
		fa->iarg = (long *)bmalloc(n*sizeof(long));
		if (fa->iarg == NULL)
			return(-1);
		for (i = 0; i < n; i++)
			if (fscanf(fp, "%ld", &fa->iarg[i]) != 1)
				return(0);
	} else
		fa->iarg = NULL;
#else
	if (n != 0)
		return(0);
#endif
	if (fscanf(fp, "%d", &n) != 1 || n < 0)
		return(0);
	if (fa->nfargs = n) {
		fa->farg = (double *)bmalloc(n*sizeof(double));
		if (fa->farg == NULL)
			return(-1);
		for (i = 0; i < n; i++)
			if (fscanf(fp, "%lf", &fa->farg[i]) != 1)
				return(0);
	} else
		fa->farg = NULL;
	return(1);
}


#ifndef  MEMHOG
freefargs(fa)		/* free object arguments */
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
