/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  header.c - routines for reading and writing information headers.
 *
 *	8/19/88
 *
 *  newheader(t,fp)	start new information header identified by string t
 *  isheadid(s)		returns true if s is a header id line
 *  headidval(r,s)	copy header identifier value in s to r
 *  printargs(ac,av,fp) print an argument list to fp, followed by '\n'
 *  isformat(s)		returns true if s is of the form "FORMAT=*"
 *  formatval(r,s)	copy the format value in s to r
 *  fputformat(s,fp)	write "FORMAT=%s" to fp
 *  getheader(fp,f,p)	read header from fp, calling f(s,p) on each line
 *  globmatch(pat, str)	check for glob match of str against pat
 *  checkheader(i,p,o)	check header format from i against p and copy to o
 *
 *  To copy header from input to output, use getheader(fin, fputs, fout)
 */

#include  <stdio.h>
#include  <ctype.h>

#define	 MAXLINE	512

#ifndef BSD
#define	 index	strchr
#endif

extern char  *index();

char  HDRSTR[] = "#?";		/* information header magic number */

char  FMTSTR[] = "FORMAT=";	/* format identifier */


newheader(s, fp)		/* identifying line of information header */
char  *s;
register FILE  *fp;
{
	fputs(HDRSTR, fp);
	fputs(s, fp);
	putc('\n', fp);
}


int
headidval(r,s)			/* get header id (return true if is id) */
register char  *r, *s;
{
	register char  *cp = HDRSTR;

	while (*cp) if (*cp++ != *s++) return(0);
	if (r == NULL) return(1);
	while (*s) *r++ = *s++;
	*r = '\0';
	return(1);
}


int
isheadid(s)			/* check to see if line is header id */
char  *s;
{
	return(headidval(NULL, s));
}


printargs(ac, av, fp)		/* print arguments to a file */
int  ac;
char  **av;
register FILE  *fp;
{
	int  quote;

	while (ac-- > 0) {
		if (index(*av, ' ') != NULL) {		/* quote it */
			if (index(*av, '\'') != NULL)
				quote = '"';
			else
				quote = '\'';
			putc(quote, fp);
			fputs(*av++, fp);
			putc(quote, fp);
		} else
			fputs(*av++, fp);
		putc(ac ? ' ' : '\n', fp);
	}
}


int
formatval(r, s)			/* get format value (return true if format) */
register char  *r;
register char  *s;
{
	register char  *cp = FMTSTR;

	while (*cp) if (*cp++ != *s++) return(0);
	while (isspace(*s)) s++;
	if (!*s) return(0);
	if (r == NULL) return(1);
	do
		*r++ = *s++;
	while(*s && !isspace(*s));
	*r = '\0';
	return(1);
}


int
isformat(s)			/* is line a format line? */
char  *s;
{
	return(formatval(NULL, s));
}


fputformat(s, fp)		/* put out a format value */
char  *s;
FILE  *fp;
{
	fputs(FMTSTR, fp);
	fputs(s, fp);
	putc('\n', fp);
}


int
getheader(fp, f, p)		/* get header from file */
FILE  *fp;
int  (*f)();
char  *p;
{
	char  buf[MAXLINE];

	for ( ; ; ) {
		buf[MAXLINE-2] = '\n';
		if (fgets(buf, MAXLINE, fp) == NULL)
			return(-1);
		if (buf[0] == '\n')
			return(0);
#ifdef MSDOS
		if (buf[0] == '\r' && buf[1] == '\n')
			return(0);
#endif
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
	char	fs[64];
};


static
mycheck(s, cp)			/* check a header line for format info. */
char  *s;
register struct check  *cp;
{
	if (!formatval(cp->fs, s) && cp->fp != NULL)
		fputs(s, cp->fp);
}


int
globmatch(pat, str)		/* check for glob match of str against pat */
char	*pat, *str;
{
	register char	*p = pat, *s = str;

	do {
		switch (*p) {
		case '?':			/* match any character */
			if (!*s++)
				return(0);
			break;
		case '*':			/* match any string */
			while (p[1] == '*') p++;
			do
				if ( (p[1]=='?' || p[1]==*s) &&
						globmatch(p+1,s) )
					return(1);
			while (*s++);
			return(0);
		case '\\':			/* literal next */
			p++;
		/* fall through */
		default:			/* normal character */
			if (*p != *s)
				return(0);
			s++;
			break;
		}
	} while (*p++);
	return(1);
}


/*
 * Checkheader(fin,fmt,fout) returns a value of 1 if the input format
 * matches the specification in fmt, 0 if no input format was found,
 * and -1 if the input format does not match or there is an
 * error reading the header.  If fmt is empty, then -1 is returned
 * if any input format is found (or there is an error), and 0 otherwise.
 * If fmt contains any '*' or '?' characters, then checkheader
 * does wildcard expansion and copies a matching result into fmt.
 * Be sure that fmt is big enough to hold the match in such cases,
 * and that it is not a static, read-only string!
 * The input header (minus any format lines) is copied to fout
 * if fout is not NULL.
 */

int
checkheader(fin, fmt, fout)
FILE  *fin;
char  *fmt;
FILE  *fout;
{
	struct check	cdat;
	register char	*cp;

	cdat.fp = fout;
	cdat.fs[0] = '\0';
	if (getheader(fin, mycheck, &cdat) < 0)
		return(-1);
	if (!cdat.fs[0])
		return(0);
	for (cp = fmt; *cp; cp++)		/* check for globbing */
		if (*cp == '?' | *cp == '*')
			if (globmatch(fmt, cdat.fs)) {
				strcpy(fmt, cdat.fs);
				return(1);
			} else
				return(-1);
	return(strcmp(fmt, cdat.fs) ? -1 : 1);	/* literal match */
}
