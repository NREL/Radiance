/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  header.c - routines for reading and writing information headers.
 *
 *	8/19/88
 */

#include  <stdio.h>


printargs(ac, av, fp)		/* print arguments to a file */
int  ac;
char  **av;
FILE  *fp;
{
	while (ac-- > 0) {
		fputs(*av++, fp);
		putc(' ', fp);
	}
	putc('\n', fp);
}


#define  MAXLINE	512

getheader(fp, f)		/* get header from file */
FILE  *fp;
int  (*f)();
{
	char  buf[MAXLINE];

	for ( ; ; ) {
		buf[MAXLINE-2] = '\n';
		if (fgets(buf, sizeof(buf), fp) == NULL)
			return(-1);
		if (buf[0] == '\n')
			return(0);
		if (buf[MAXLINE-2] != '\n') {
			ungetc(buf[MAXLINE-2], fp);	/* prevent false end */
			buf[MAXLINE-2] = '\0';
		}
		if (f != NULL)
			(*f)(buf);
	}
}


static FILE	*outfp;

static
myputs(s)
char  *s;
{
	fputs(s, outfp);
}


copyheader(fin, fout)		/* copy file header */
FILE  *fin, *fout;
{
	outfp = fout;
	return(getheader(fin, myputs));
}
