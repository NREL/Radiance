/* Copyright (c) 1990 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *	Miscellaneous definitions required by many routines.
 */

#include  <stdio.h>

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

#ifdef  M_PI
#define  PI		M_PI
#else
#define  PI		3.14159265358979323846
#endif

#ifndef  F_OK			/* mode bits for access(2) call */
#define  R_OK		4		/* readable */
#define  W_OK		2		/* writable */
#define  X_OK		1		/* executable */
#define  F_OK		0		/* exists */
#endif
				/* error codes */
#define  WARNING	1		/* non-fatal error */
#define  USER		2		/* fatal user-caused error */
#define  SYSTEM		3		/* fatal system-related error */
#define  INTERNAL	4		/* fatal program-related error */
#define  CONSISTENCY	5		/* bad consistency check, abort */
#define  COMMAND	6		/* interactive error */

extern char  errmsg[];			/* global buffer for error messages */

extern int  errno;			/* system error number */

					/* memory operations */
#ifdef  STRUCTASSIGN
#define  copystruct(d,s)	(*(d) = *(s))
#else
#define  copystruct(d,s)	bcopy((char *)(s),(char *)(d),sizeof(*(d)))
#endif

#ifndef  BSD
#define  bcopy(s,d,n)		(void)memcpy(d,s,n)
#define  bzero(d,n)		(void)memset(d,0,n)
#define  bcmp(b1,b2,n)		memcmp(b1,b2,n)
extern char  *memcpy(), *memset();
#define  index			strchr
#define  rindex			strrchr
#endif

extern char  *sskip();
extern char  *getpath(), *getenv();
extern char  *malloc(), *calloc(), *realloc();
extern char  *bmalloc(), *savestr(), *savqstr();
