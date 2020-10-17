#ifndef lint
static const char RCSid[] = "$Id: readobj.c,v 2.24 2020/10/17 16:39:23 greg Exp $";
#endif
/*
 *  readobj.c - routines for reading in object descriptions.
 *
 *  External symbols declared in object.h
 */

#include "copyright.h"

#include  <ctype.h>
#include  <string.h>
#include  <stdio.h>

#include  "platform.h"
#include  "paths.h"
#include  "standard.h"
#include  "object.h"
#include  "otypes.h"


OBJREC  *objblock[MAXOBJBLK];		/* our objects */
OBJECT  nobjects = 0;			/* # of objects */


void
readobj(				/* read in an object file or stream */
	char  *inpspec
)
{
	OBJECT  lastobj;
	FILE  *infp;
	char  buf[2048];
	int  c;

	lastobj = nobjects;
	if (inpspec == NULL) {
		infp = stdin;
		inpspec = "standard input";
	} else if (inpspec[0] == '!') {
		if ((infp = popen(inpspec+1, "r")) == NULL) {
			sprintf(errmsg, "cannot execute \"%s\"", inpspec);
			error(SYSTEM, errmsg);
		}
	} else if ((infp = fopen(inpspec, "r")) == NULL) {
		sprintf(errmsg, "cannot open scene file \"%s\"", inpspec);
		error(SYSTEM, errmsg);
	}
	while ((c = getc(infp)) != EOF) {
		if (isspace(c))
			continue;
		if (c == '#') {				/* comment */
			fgets(buf, sizeof(buf), infp);
		} else if (c == '!') {			/* command */
			ungetc(c, infp);
			fgetline(buf, sizeof(buf), infp);
			readobj(buf);
		} else {				/* object */
			ungetc(c, infp);
			getobject(inpspec, infp);
		}
	}
	if (inpspec[0] == '!')
		pclose(infp);
	else if (infp != stdin)
		fclose(infp);
	if (nobjects == lastobj) {
		sprintf(errmsg, "(%s): empty file", inpspec);
		error(WARNING, errmsg);
	}
}


void
getobject(				/* read the next object */
	char  *name,
	FILE  *fp
)
{
#define	OALIAS	-2
	OBJECT  obj;
	char  sbuf[MAXSTR];
	int  rval;
	OBJREC  *objp;

	if ((obj = newobject()) == OVOID)
		error(SYSTEM, "out of object space");
	objp = objptr(obj);
					/* get modifier */
	strcpy(sbuf, "EOF");
	fgetword(sbuf, MAXSTR, fp);
	if (strchr(sbuf, '\t')) {
		sprintf(errmsg, "(%s): illegal tab in modifier \"%s\"",
					name, sbuf);
		error(USER, errmsg);
	}
	if (!strcmp(sbuf, VOIDID))
		objp->omod = OVOID;
	else if (!strcmp(sbuf, ALIASMOD))
		objp->omod = OALIAS;
	else if ((objp->omod = modifier(sbuf)) == OVOID) {
		sprintf(errmsg, "(%s): undefined modifier \"%s\"", name, sbuf);
		error(USER, errmsg);
	}
					/* get type */
	strcpy(sbuf, "EOF");
	fgetword(sbuf, MAXSTR, fp);
	if ((objp->otype = otype(sbuf)) < 0) {
		sprintf(errmsg, "(%s): unknown type \"%s\"", name, sbuf);
		error(USER, errmsg);
	}
					/* get identifier */
	sbuf[0] = '\0';
	fgetword(sbuf, MAXSTR, fp);
	if (strchr(sbuf, '\t')) {
		sprintf(errmsg, "(%s): illegal tab in identifier \"%s\"",
					name, sbuf);
		error(USER, errmsg);
	}
	objp->oname = savqstr(sbuf);
					/* get arguments */
	if (objp->otype == MOD_ALIAS) {
		OBJECT  alias;
		strcpy(sbuf, "EOF");
		fgetword(sbuf, MAXSTR, fp);
		if ((alias = modifier(sbuf)) == OVOID) {
			sprintf(errmsg, "(%s): bad reference \"%s\"",
					name, sbuf);
			objerror(objp, USER, errmsg);
		}
		if (objp->omod == OALIAS || 
				objp->omod == objptr(alias)->omod) {
			objp->omod = alias;
		} else {
			objp->oargs.sarg = (char **)malloc(sizeof(char *));
			if (objp->oargs.sarg == NULL)
				error(SYSTEM, "out of memory in getobject");
			objp->oargs.nsargs = 1;
			objp->oargs.sarg[0] = savestr(sbuf);
		}
	} else if ((rval = readfargs(&objp->oargs, fp)) == 0) {
		sprintf(errmsg, "(%s): bad arguments", name);
		objerror(objp, USER, errmsg);
	} else if (rval < 0) {
		sprintf(errmsg, "(%s): error reading scene", name);
		error(SYSTEM, errmsg);
	}
	if (objp->omod == OALIAS) {
		sprintf(errmsg, "(%s): inappropriate use of '%s' modifier",
				name, ALIASMOD);
		objerror(objp, USER, errmsg);
	}
					/* initialize */
	objp->os = NULL;

	insertobject(obj);		/* add to global structure */
#undef OALIAS
}


OBJECT
newobject(void)				/* get a new object */
{
	int  i;

	if ((nobjects & (OBJBLKSIZ-1)) == 0) {	/* new block */
		errno = 0;
		i = nobjects >> OBJBLKSHFT;
		if (i >= MAXOBJBLK)
			return(OVOID);
		objblock[i] = (OBJREC *)calloc(OBJBLKSIZ, sizeof(OBJREC));
		if (objblock[i] == NULL)
			return(OVOID);
	}
	return(nobjects++);
}

void
freeobjects(				/* free a range of objects */
	int firstobj,
	int nobjs
)
{
	int  obj;
					/* check bounds */
	if (firstobj < 0)
		return;
	if (nobjs <= 0)
		return;
	if (firstobj + nobjs > nobjects)
		return;
					/* clear objects */
	for (obj = firstobj+nobjs; obj-- > firstobj; ) {
		OBJREC  *o = objptr(obj);
		free_os(o);		/* free client memory */
		freeqstr(o->oname);
		freefargs(&o->oargs);
		memset((void *)o, '\0', sizeof(OBJREC));
	}
					/* free objects off end */
	for (obj = nobjects; obj-- > 0; )
		if (objptr(obj)->oname != NULL)
			break;
	if (++obj >= nobjects)
		return;
	while (nobjects > obj)		/* free empty end blocks */
		if ((--nobjects & (OBJBLKSIZ-1)) == 0) {
			int	i = nobjects >> OBJBLKSHFT;
			free((void *)objblock[i]);
			objblock[i] = NULL;
		}
	truncobjndx();			/* truncate modifier look-up */
}
