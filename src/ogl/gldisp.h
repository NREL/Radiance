/* Copyright (c) 1997 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for OpenGL display process.
 */

#ifndef	MEM_PTR
#define MEM_PTR		char *
#endif
#ifndef int4
#define	int4		int		/* assume 32-bit integers */
#endif
#undef	NOPROTO
#define	NOPROTO		1

#include	"color.h"

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

				/* stream argument lengths */
#define	GD_ARGLEN	{0,8,4,4,8,4,5,-1,1,1}
				/* array element lengths (0 == unsupported) */
#define GD_ELELEN	{0,8,4,4,8,4,sizeof(GDarg),sizeof(char *),0,0}

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
		COLR		c;		/* 4-byte RGBE color */
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

#define GD_HSIZ		123		/* hash table size (prime) */
extern GDrequest	*gdProTab[GD_HSIZ];	/* registered prototypes */

#define gdHash(a)	((a)->v.n1 ^ (a)->v.n2)

#define gdFree(p)	free((MEM_PTR)(p))

typedef struct {
	int	xres, yres;	/* display window size (renderable area) */
} GDserv;			/* display structure */
