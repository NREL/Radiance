/* Copyright (c) 1999 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  random.h - header file for random(3) and urand() function.
 *
 *     10/1/85
 */

#ifdef	MSDOS

#define random()	((long)rand()<<16^(long)rand()<<6^(long)rand()>>4)
#define srandom(s)	srand((unsigned)(s))

#define frandom()	(rand()*(1./32768.))

#else
#ifdef	BSD

extern long  random();

#define	 frandom()	(random()*(1./2147483648.))

#else

extern long  lrand48();
extern double  drand48();

#define	 random()	lrand48()
#define  srandom(s)	srand48((long)(s))
#define	 frandom()	drand48()

#endif
#endif

#ifdef  MC

#define  urand(i)	frandom()
#define  initurand(n)	(n)

#else

extern unsigned short	*urperm;
extern int	urmask, initurand();

#define	 urand(i)	((urperm[(i)&urmask]+frandom())/(urmask+1))

#endif
