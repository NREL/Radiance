/* Copyright (c) 1995 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/* Pmap return codes */
#define PMAP_BAD	-1
#define PMAP_LINEAR	0
#define PMAP_PERSP	1

/*  |a b|
 *  |c d|
 */
#define DET2(a,b, c,d) ((a)*(d) - (b)*(c))
