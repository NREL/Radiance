/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  error.c - standard error reporting function
 */

#include  "standard.h"

extern char  *sys_errlist[];	/* system error list */
extern int  sys_nerr;		/* number of system errors */

char  errmsg[512];		/* global error message buffer */


error(etype, emsg)		/* report error, quit if necessary */
int  etype;
char  *emsg;
{
	switch (etype) {
	case WARNING:
		wputs("warning - ");
		wputs(emsg);
		wputs("\n");
		return;
	case COMMAND:
		cputs(emsg);
		cputs("\n");
		return;
	case USER:
		eputs("fatal - ");
		eputs(emsg);
		eputs("\n");
		quit(1);
	case INTERNAL:
		eputs("internal - ");
		eputs(emsg);
		eputs("\n");
		quit(1);
	case SYSTEM:
		eputs("system - ");
		eputs(emsg);
		if (errno > 0) {
			eputs(": ");
			if (errno <= sys_nerr)
				eputs(sys_errlist[errno]);
			else
				eputs("Unknown error");
		}
		eputs("\n");
		quit(2);
	case CONSISTENCY:
		eputs("consistency - ");
		eputs(emsg);
		eputs("\n");
		abort();
	}
}
