/* Copyright 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

#include  <stdio.h>


int
fwrite(ptr, size, count, fp)		/* system version is broken (*sigh*) */
register char  *ptr;
int  size, count;
register FILE  *fp;
{
    register long  n = size*count;

    while (n--)
	putc(*ptr++, fp); 

    return(ferror(fp) ? 0 : count);
}
