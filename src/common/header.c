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

#define  MAXLINE	512

char  FMTSTR[] = "FORMAT=";
int  FMTSTRL = 7;


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


isformat(s)			/* is line a format line? */
char  *s;
{
	return(!strncmp(s,FMTSTR,FMTSTRL));
}


formatval(r, s)			/* return format value */
register char  *r;
register char  *s;
{
	s += FMTSTRL;
	while (*s && *s != '\n')
		*r++ = *s++;
	*r = '\0';
}


fputformat(s, fp)		/* put out a format value */
char  *s;
FILE  *fp;
{
	fputs(FMTSTR, fp);
	fputs(s, fp);
	putc('\n', fp);
}


getheader(fp, f, p)		/* get header from file */
FILE  *fp;
int  (*f)();
char  *p;
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
			(*f)(buf, p);
	}
}


struct check {
	FILE	*fp;
	char	fs[32];
};


static
mycheck(s, cp)			/* check a header line for format info. */
char  *s;
register struct check  *cp;
{
	if (!strncmp(s,FMTSTR,FMTSTRL))
		formatval(cp->fs, s);
	else if (cp->fp != NULL)	/* don't copy format info. */
		fputs(s, cp->fp);
}


checkheader(fin, fmt, fout)	/* check data format in header */
FILE  *fin;
char  *fmt;
FILE  *fout;
{
	struct check	cdat;

	cdat.fp = fout;
	cdat.fs[0] = '\0';
	if (getheader(fin, mycheck, &cdat) < 0)
		return(-1);
	if (fmt != NULL && cdat.fs[0] != '\0')
		return(strcmp(fmt, cdat.fs) ? -1 : 1);
	return(0);
}
