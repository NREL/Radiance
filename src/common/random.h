/* Copyright (c) 1992 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  random.h - header file for random(3) and urand() function.
 *
 *     10/1/85
 */

#ifdef	MSDOS

#define random()	((long)rand()<<16^(long)rand()<<6^(long)rand()>>4)

#define frandom()	(rand()*(1./32768.))

#else
#ifdef	BSD

extern long  random();

#define	 frandom()	(random()*(1./2147483648.))

#else

extern long  lrand48();
extern double  drand48();

#define	 random()	lrand48()
#define	 frandom()	drand48()

#endif
#endif

#ifdef  MC
#define  urand(i)	frandom()
#else
#define	 urand(i)	((urperm[(i)&urmask]+frandom())/(urmask+1))
#endif

extern short  *urperm;
extern int  urmask;
