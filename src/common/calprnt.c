/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  calprint.c - routines for printing calcomp expressions.
 *
 *     1/29/87
 */

#include  <stdio.h>

#include  "calcomp.h"


eprint(ep, fp)			/* print a parse tree */
register EPNODE  *ep;
FILE  *fp;
{
    static EPNODE  *curdef = NULL;
    register EPNODE  *ep1;

    switch (ep->type) {

	case VAR:
	    fputs(ep->v.ln->name, fp);
	    break;

	case SYM:
	    fputs(ep->v.name, fp);
	    break;

	case FUNC:
	    eprint(ep->v.kid, fp);
	    fputs("(", fp);
	    ep1 = ep->v.kid->sibling;
	    while (ep1 != NULL) {
		eprint(ep1, fp);
		if ((ep1 = ep1->sibling) != NULL)
		    fputs(", ", fp);
	    }
	    fputs(")", fp);
	    break;

	case ARG:
	    if (curdef == NULL || curdef->v.kid->type != FUNC ||
			(ep1 = ekid(curdef->v.kid, ep->v.chan)) == NULL) {
		eputs("Bad argument!\n");
		quit(1);
	    }
	    eprint(ep1, fp);
	    break;

	case NUM:
	    fprintf(fp, "%.9g", ep->v.num);
	    break;

	case UMINUS:
	    if (ep->v.kid->type == UMINUS) {
		fputs("-(", fp);
	        eprint(ep->v.kid, fp);
		fputs(")", fp);
	    } else {
		fputs("-", fp);
		eprint(ep->v.kid, fp);
	    }
	    break;

	case CHAN:
	    fprintf(fp, "$%d", ep->v.chan);
	    break;
	
	case '=':
	case ':':
	    ep1 = curdef;
	    curdef = ep;
	    eprint(ep->v.kid, fp);
	    putc(' ', fp);
	    putc(ep->type, fp);
	    putc(' ', fp);
	    eprint(ep->v.kid->sibling, fp);
	    curdef = ep1;
	    break;
	    
	case '+':
	case '-':
	case '*':
	case '/':
	case '^':
	    fputs("(", fp);
	    eprint(ep->v.kid, fp);
	    putc(' ', fp);
	    putc(ep->type, fp);
	    putc(' ', fp);
	    eprint(ep->v.kid->sibling, fp);
	    fputs(")", fp);
	    break;

	default:
	    eputs("Bad expression!\n");
	    quit(1);

    }

}


dprint(name, fp)		/* print a definition (all if no name) */
char  *name;
FILE  *fp;
{
    register EPNODE  *ep;
    
    if (name == NULL)
	for (ep = dfirst(); ep != NULL; ep = dnext()) {
	    eprint(ep, fp);
	    fputs(";\n", fp);
	}
    else if ((ep = dlookup(name)) != NULL) {
	eprint(ep, fp);
	fputs(";\n", fp);
    } else {
	wputs(name);
	wputs(": undefined\n");
    }
}
