/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * popen() and pclose() calls for systems without pipe facilities
 */
 
#include <stdio.h>
#include <ctype.h>
#include "paths.h"

#ifndef NFILE
#define NFILE		32
#endif

static struct {
	char	*f;		/* file name, NULL if inactive */
	char	*c;		/* command if writing */
} pips[NFILE];


FILE *
popen(cmd, mode)		/* open command for reading or writing */
register char	*cmd;
char	*mode;
{
	extern FILE	*fopen();
	extern char	*malloc(), *mktemp(), *strcpy();
	static int	fnum = 0;
	FILE	*fp;
	char	*newcmd, *fname;
	register char	*cp, *cp2 = NULL;
	int	quote = '\0', paren = 0;
	
	fname = malloc(TEMPLEN+1);
	newcmd = malloc(TEMPLEN+6+strlen(cmd));
	if (fname == NULL | newcmd == NULL)
		return(NULL);
	mktemp(strcpy(fname,TEMPLATE));
				/* build our command */
	for (cp = newcmd; ; cmd++) {
		switch (*cmd) {
		case '\'':
		case '"':
			if (!quote)
				quote = *cmd;
			else if (quote == *cmd)
				quote = '\0';
#ifdef MSDOS
			else
				break;
			*cp++ = '"';		/* double quotes only */
			continue;
#else
			break;
#endif		      
		case '(':
			if (!quote)
				paren++;
			break;
		case ')':
			if (!quote && paren)
				paren--;
			break;
		case '\\':
			if (!quote && cmd[1] == '\n') {
				*cp++ = ' ';
				cmd++;
				continue;
			}
			*cp++ = *cmd++;
			break;
		case ' ':
		case '\t':
			if (!quote)
				while (isspace(cmd[1]))
					cmd++;
			break;
		case '|':
		case ';':
			if (!quote && !paren && cp2 == NULL)
				cp2 = fname;
			break;
		case '\n':
		case '\0':
			if (cp2 == NULL)
				cp2 = fname;
			break;
		}
		if (cp2 == fname && *mode == 'w') {	/* add input file */
			*cp++ = ' '; *cp++ = '<'; *cp++ = ' ';
			while (*cp2)
				*cp++ = *cp2++;
			*cp++ = ' ';
		}
		if (!(*cp = *cmd))
			break;
		cp++;
	}
	if (*mode == 'r') {			/* add output file */
		while (isspace(cp[-1]) || cp[-1] == ';')
			cp--;
		*cp++ = ' '; *cp++ = '>'; *cp++ = ' ';
		while (*cp2)
			*cp++ = *cp2++;
		*cp = '\0';
		system(newcmd);			/* execute command first */
	}
	if ((fp = fopen(fname, mode)) == NULL) {	/* open file */
		free(fname);
		free(newcmd);
		return(NULL);
	}
	if (fileno(fp) >= NFILE) {
		fputs("popen: too many open files\n", stderr);
		exit(1);
	}
	pips[fileno(fp)].f = fname;
	if (*mode == 'r') {
		free(newcmd);
		pips[fileno(fp)].c = NULL;
	} else
		pips[fileno(fp)].c = newcmd;
	return(fp);
}


pclose(fp)			/* close pipe and cleanup */
FILE	*fp;
{
	register int	pn = fileno(fp);

	if (pn >= NFILE || pips[pn].f == NULL || fclose(fp) == -1)
		return(-1);
	if (pips[pn].c != NULL) {
		system(pips[pn].c);		/* execute command last */
		free(pips[pn].c);
	}
	unlink(pips[pn].f);		/* remove temp file */
	free(pips[pn].f);
	pips[pn].f = NULL;
	return(0);
}
