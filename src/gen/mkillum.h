/* Copyright (c) 1991 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Common definitions for mkillum
 */
				/* illum flags */
#define  IL_FLAT	0x1		/* flat surface */
#define  IL_LIGHT	0x2		/* light rather than illum */
#define  IL_COLDST	0x4		/* colored distribution */
#define  IL_COLAVG	0x8		/* compute average color */
#define  IL_DATCLB	0x10		/* OK to clobber data file */

struct illum_args {
	int	flags;			/* flags from list above */
	char	matname[MAXSTR];	/* illum material name */
	char	datafile[MAXSTR];	/* distribution data file name */
	int	dfnum;			/* data file number */
	char	altmatname[MAXSTR];	/* alternate material name */
	int	nsamps;			/* # of samples in each direction */
	int	nalt, nazi;		/* # of altitude and azimuth angles */
	FVECT	orient;			/* coordinate system orientation */
};				/* illum options */

struct rtproc {
	int	pd[3];			/* rtrace pipe descriptors */
	float	*buf;			/* rtrace i/o buffer */
	int	bsiz;			/* maximum rays for rtrace buffer */
	int	nrays;			/* current length of rtrace buffer */
};				/* rtrace process */
