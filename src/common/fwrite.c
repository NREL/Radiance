#include  <stdio.h>

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif


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
