/* Copyright (c) 1990 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Definitions for 4x4 matrix operations
 */

#include  "fvect.h"

typedef FLOAT  MAT4[4][4];

#ifdef  BSD
#define  copymat4(m4a,m4b)	bcopy((char *)m4b,(char *)m4a,sizeof(MAT4))
#else
#define  copymat4(m4a,m4b)	(void)memcpy((char *)m4a,(char *)m4b,sizeof(MAT4))
extern char  *memcpy();
#endif

#define  MAT4IDENT		{ 1.,0.,0.,0., 0.,1.,0.,0., \
				0.,0.,1.,0., 0.,0.,0.,1. }

extern MAT4  m4ident;

#define  setident4(m4)		copymat4(m4, m4ident)
