/* Copyright (c) 1997 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *	Miscellaneous definitions required by many routines.
 */

#include  <stdio.h>

#include  <fcntl.h>

#include  <math.h>

#include  <errno.h>

#include  "mat4.h"
				/* regular transformation */
typedef struct {
	MAT4  xfm;				/* transform matrix */
	FLOAT  sca;				/* scalefactor */
}  XF;
				/* complemetary tranformation */
typedef struct {
	XF  f;					/* forward */
	XF  b;					/* backward */
}  FULLXF;

#ifndef  PI
#ifdef	M_PI
#define	 PI		M_PI
#else
#define	 PI		3.14159265358979323846
#endif
#endif

#ifndef	 F_OK			/* mode bits for access(2) call */
#define	 R_OK		4		/* readable */
#define	 W_OK		2		/* writable */
#define	 X_OK		1		/* executable */
#define	 F_OK		0		/* exists */
#endif

extern int  eputs(), wputs();	/* standard error output functions */

				/* error codes */
#define	 WARNING	0		/* non-fatal error */
#define	 USER		1		/* fatal user-caused error */
#define	 SYSTEM		2		/* fatal system-related error */
#define	 INTERNAL	3		/* fatal program-related error */
#define	 CONSISTENCY	4		/* bad consistency check, abort */
#define	 COMMAND	5		/* interactive error */
#define  NERRS		6
				/* error struct */
extern struct erract {
	char	pre[16];		/* prefix message */
	int	(*pf)();		/* put function (resettable) */
	int	ec;			/* exit code (0 means non-fatal) */
} erract[NERRS];	/* list of error actions */

#define  ERRACT_INIT	{	{"warning - ", wputs, 0}, \
				{"fatal - ", eputs, 1}, \
				{"system - ", eputs, 2}, \
				{"internal - ", eputs, 1}, \
				{"consistency - ", eputs, -1}, \
				{"", NULL, 0}	}

extern char  errmsg[];			/* global buffer for error messages */

					/* memory operations */
#ifdef	NOSTRUCTASS
#define	 copystruct(d,s)	bcopy((char *)(s),(char *)(d),sizeof(*(d)))
#else
#define	 copystruct(d,s)	(*(d) = *(s))
#endif

#ifndef	 BSD
#define	 bcopy(s,d,n)		(void)memcpy(d,s,n)
#define	 bzero(d,n)		(void)memset(d,0,n)
#define	 bcmp(b1,b2,n)		memcmp(b1,b2,n)
#define	 index			strchr
#define	 rindex			strrchr
#endif

extern char  *sskip(), *sskip2();
extern char  *getpath(), *getenv();
#ifndef malloc
extern char  *malloc(), *calloc(), *realloc();
#endif
extern char  *bmalloc(), *savestr(), *savqstr();

#ifdef  DCL_ATOF
extern double  atof();
#endif

#ifdef MSDOS
#define NIX 1
#endif
#ifdef AMIGA
#define NIX 1
#endif

