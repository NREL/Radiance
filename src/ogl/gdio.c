/* Copyright (c) 1997 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Handle data stream to and from GL display process.
 */

#include "standard.h"
#include <string.h>
#include "gldisp.h"


#define GD_TYPELEN	1		/* a type is 1 byte in length */

				/* argument types */
#define GD_TY_END	0		/* argument list terminator */
#define GD_TY_NAM	1		/* 8-byte (max.) identifier */
#define	GD_TY_INT	2		/* 4-byte integer */
#define GD_TY_FLT	3		/* 4-byte IEEE float */
#define GD_TY_DBL	4		/* 8-byte IEEE double */
#define GD_TY_CLR	5		/* 4-byte RGBE color value */
#define GD_TY_ARR	6		/* 5-byte array prefix */
#define GD_TY_STR	7		/* nul-terminated string */
#define GD_TY_ARG	8		/* 1-byte argument list prefix */
#define GD_TY_ERR	9		/* 1-byte error code */

#define	GD_NTYPES	10		/* number of argument types */

				/* argument lengths */
#define	GD_ARGLEN	{0,8,4,4,8,4,5,-1,1,1}

#define GD_MAXID	8		/* maximum id. length (must be 8) */

/*
 * A request consists of an argument list, the first of
 * which is always the request name as an 8-char (max.) string.
 * The types of the following arguments must match the required
 * arguments of the display request, or an error will result.
 *
 * Only functions return values, and they return them as a argument
 * list on the client's receiving connection.  It is up to the client
 * program to keep track of its function calls and which values correspond
 * to which request functions.
 *
 * An error is indicated with a special GD_TY_ERR code on the receiving
 * connection, and usually indicates something fatal.
 */

				/* error codes */
#define GD_ER_UNRECOG	0		/* unrecognized request */
#define GD_ER_ARGTYPE	1		/* argument type mismatch */
#define GD_ER_ARGMISS	2		/* argument(s) missing */

#define GD_NERRS	3		/* number of errors */

				/* request argument */
typedef struct {
	BYTE		typ;		/* argument type */
	BYTE		atyp;		/* array subtype if typ==GD_TY_ARR */
	union {
		char		n[GD_MAXID];	/* 8-char (max.) id. */
		int4		n1, n2;		/* used for ID comparison */
		int4		i;		/* 4-byte integer */
		float		f;		/* 4-byte IEEE float */
		double		d;		/* 8-byte IEEE double */
		COLR		c;		/* RGBE */
		struct array {	
			int4		l;		/* length */
			MEM_PTR		p;		/* values */
		}		a;		/* array */
		char		*s;		/* nul-terminated string */
	}		v;		/* argument value */
}	GDarg;

				/* a request and its arguments */
typedef struct gdreq {
	struct gdreq	*next;		/* next request in list */
	short		argc;		/* number of arguments */
	GDarg		argv[1];	/* argument list (expandable) */
}	GDrequest;

GDrequest	*gdProTab[GD_HSIZ];	/* registered prototypes */


gdRegProto(r)			/* register a request prototype */
register GDrequest	*r;
{
	register int4	hval;

	hval = gdHash(r->argv);
	r->next = gdProTab[hval];
	gdProTab[hval] = r;
}


GDrequest *
gdReqAlloc(nm, ac)		/* allocate a request and its arguments */
char	*nm;
int	ac;
{
	register GDrequest	*nr;

	if (ac < 1)
		return(NULL);
	nr = (GDrequest *)malloc(sizeof(GDrequest)+(ac-1)*sizeof(GDarg));
	if (nr == NULL)
		return(NULL);
	nr->next = NULL;
	nr->argc = ac;
	if (nm != NULL)
		(void)strncmp(rn->argv[0].v.n, nm, GD_MAXID);
	return(nr);
}


char *
gdStrAlloc(a, str)		/* allocate and save a string value */
register GDarg	*a;
char	*str;
{
	a->typ = GD_TY_STR;
	a->v.s = (char *)malloc(strlen(str)+1);
	if (a->v.s == NULL)
		return(NULL);
	return(strcpy(a->v.s, str));
}


MEM_PTR
gdArrAlloc(a, typ, len, arr)	/* allocate and assign an array */
register GDarg	*a;
int	typ, len;
MEM_PTR	arr;
{
	static short	esiz[GD_NTYPES] = GD_ELELEN;

	if (esiz[typ] <= 0)
		return(NULL);
	a->v.a.p = (MEM_PTR)malloc(len*esiz[typ]);
	if (a->v.a.p == NULL)
		return(NULL);
	a->typ = GD_TY_ARR;
	a->atyp = typ;
	a->v.a.l = len;
	if (arr != NULL)
		(void)memcpy((char *)a->v.a.p, (char *)arr, len*esiz[typ]);
	return(a->v.a.p);
}


gdDoneArg(a)			/* free any argument data */
register GDarg	*a;
{
	register int	j;
					/* free allocated arrays */
	switch (a->typ) {
	case GD_TY_ARR:				/* array of... */
		if (a->atyp == GD_TY_ARR) {		/* arrays */
			for (j = a->v.a.l; j--; )
				gdDoneArg((GDarg *)a->v.a.p + j);
		} else if (a->atyp == GD_TY_STR) {	/* strings */
			for (j = r->v.a.l; j--; )
				gdFree((char **)r->v.a.p + j);
		}
		gdFree(r->argv[i].v.a.p);
		break;
	case GD_TY_STR:				/* string value */
		gdFree(a->v.s);
		break;
	}
}


gdFreeReq(r)			/* free a request */
register GDrequest	*r;
{
	register int	i;
					/* free any argument data */
	for (i = r->argc; i--; )
		gdDoneArg(r->argv + i);
	gdFree(r);			/* free basic structure */
}
