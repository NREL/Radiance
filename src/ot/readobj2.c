/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  readobj2.c - routines for reading in object descriptions.
 */

#include  "standard.h"

#include  "object.h"

#include  "otypes.h"

#include  <ctype.h>

extern char  *fgetword();

OBJREC  *objblock[MAXOBJBLK];		/* our objects */
OBJECT  nobjects = 0;			/* # of objects */
int newobject() {return(0);}


readobj(input, callback)	/* read in an object file or stream */
char  *input;
int  (*callback)();
{
	FILE  *popen();
	char  *fgetline();
	FILE  *infp;
	char  buf[512];
	register int  c;

	if (input == NULL) {
		infp = stdin;
		input = "standard input";
	} else if (input[0] == '!') {
		if ((infp = popen(input+1, "r")) == NULL) {
			sprintf(errmsg, "cannot execute \"%s\"", input);
			error(SYSTEM, errmsg);
		}
	} else if ((infp = fopen(input, "r")) == NULL) {
		sprintf(errmsg, "cannot open scene file \"%s\"", input);
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
			readobj(buf, callback);
		} else {				/* object */
			ungetc(c, infp);
			getobject(input, infp, callback);
		}
	}
	if (input[0] == '!')
		pclose(infp);
	else
		fclose(infp);
}


getobject(name, fp, f)			/* read the next object */
char  *name;
FILE  *fp;
int  (*f)();
{
	char  sbuf[MAXSTR];
	OBJREC  thisobj;
					/* get modifier */
	fgetword(sbuf, MAXSTR, fp);
	thisobj.omod = OVOID;
					/* get type */
	fgetword(sbuf, MAXSTR, fp);
	if (!strcmp(sbuf, ALIASID))
		thisobj.otype = -1;
	else if ((thisobj.otype = otype(sbuf)) < 0) {
		sprintf(errmsg, "(%s): unknown type \"%s\"", name, sbuf);
		error(USER, errmsg);
	}
					/* get identifier */
	fgetword(sbuf, MAXSTR, fp);
	thisobj.oname = sbuf;
					/* get arguments */
	if (thisobj.otype == -1) {
		fscanf(fp, "%*s");
		return;
	}
	if (readfargs(&thisobj.oargs, fp) <= 0) {
		sprintf(errmsg, "(%s): bad arguments", name);
		objerror(&thisobj, USER, errmsg);
	}
	thisobj.os = NULL;
					/* call function */
	(*f)(&thisobj);
					/* free memory */
	if (thisobj.os != NULL)
		switch (thisobj.otype) {
		case OBJ_CONE:
		case OBJ_CUP:
		case OBJ_CYLINDER:
		case OBJ_TUBE:
		case OBJ_RING:
			freecone(&thisobj);
			break;
		case OBJ_FACE:
			freeface(&thisobj);
			break;
		case OBJ_INSTANCE:
			freeinstance(&thisobj);
			break;
		}
	freefargs(&thisobj.oargs);
}
