/* Copyright (c) 1991 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for linear regression calculation.
 */

typedef struct {
	double	xs, ys, xxs, yys, xys;
	int	n;
} LRSUM;

typedef struct {
	double	slope, intercept, correlation;
} LRLIN;

extern double	sqrt();

#define	lrpoint(x,y,l)	((l)->xs+=(x),(l)->ys+=(y),(l)->xxs+=(x)*(x), \
			(l)->yys+=(y)*(y),(l)->xys+=(x)*(y),++(l)->n)

#define	lrxavg(l)	((l)->xs/(l)->n)
#define	lryavg(l)	((l)->ys/(l)->n)
#define	lrxvar(l)	(((l)->xxs-(l)->xs*(l)->xs/(l)->n)/(l)->n)
#define	lryvar(l)	(((l)->yys-(l)->ys*(l)->ys/(l)->n)/(l)->n)
#define	lrxdev(l)	sqrt(((l)->xxs-(l)->xs*(l)->xs/(l)->n)/((l)->n-1))
#define	lrydev(l)	sqrt(((l)->yys-(l)->ys*(l)->ys/(l)->n)/((l)->n-1))
