/* Copyright (c) 1993 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Duplicate header on stdout.
 */

#include  "standard.h"
#include  "paths.h"


int  headismine = 1;		/* true if header file belongs to me */

static char  *headfname = NULL;	/* temp file name */
static FILE  *headfp = NULL;	/* temp file pointer */


headclean()			/* remove header temp file (if one) */
{
	if (headfname != NULL) {
		if (headfp != NULL)
			fclose(headfp);
		if (headismine)
			unlink(headfname);
	}
}


openheader()			/* save standard output to header file */
{
	headfname = mktemp(TEMPLATE);
	if (freopen(headfname, "w", stdout) == NULL) {
		sprintf(errmsg, "cannot open header file \"%s\"", headfname);
		error(SYSTEM, errmsg);
	}
}


dupheader()			/* repeat header on standard output */
{
	register int  c;

	if (headfp == NULL) {
		if ((headfp = fopen(headfname, "r")) == NULL)
			error(SYSTEM, "error reopening header file");
#ifdef MSDOS
		setmode(fileno(headfp), O_BINARY);
#endif
	} else if (fseek(headfp, 0L, 0) < 0)
		error(SYSTEM, "seek error on header file");
	while ((c = getc(headfp)) != EOF)
		putchar(c);
}
