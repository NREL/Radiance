/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  random.h - header file for random(3) function.
 *
 *     10/1/85
 */

#ifdef  BSD

extern long  random();

#define  frandom()	(random()*(1./2147483648.))

#else

extern long  lrand48();
extern double  drand48();

#define  random()	lrand48()
#define  frandom()	drand48()

#endif
