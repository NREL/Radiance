/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  calfunc.c - routines for calcomp using functions.
 *
 *	The define BIGLIB pulls in a large number of the
 *  available math routines.
 *
 *      If VARIABLE is not defined, only library functions
 *  can be accessed.
 *
 *     4/2/86
 */

#include  <stdio.h>

#include  <errno.h>

#include  "calcomp.h"

				/* bits in argument flag (better be right!) */
#define  AFLAGSIZ	(8*sizeof(unsigned long))
#define  ALISTSIZ	6	/* maximum saved argument list */

typedef struct activation {
    char  *name;		/* function name */
    struct activation  *prev;	/* previous activation */
    double  *ap;		/* argument list */
    unsigned long  an;		/* computed argument flags */
    EPNODE  *fun;		/* argument function */
}  ACTIVATION;		/* an activation record */

static ACTIVATION  *curact = NULL;

static double  libfunc();

#define  MAXLIB		64	/* maximum number of library functions */

static double  l_if(), l_select(), l_rand();
static double  l_floor(), l_ceil();
#ifdef  BIGLIB
static double  l_sqrt();
static double  l_sin(), l_cos(), l_tan();
static double  l_asin(), l_acos(), l_atan(), l_atan2();
static double  l_exp(), l_log(), l_log10();
#endif

#ifdef  BIGLIB
			/* functions must be listed alphabetically */
static LIBR  library[MAXLIB] = {
    { "acos", 1, ':', l_acos },
    { "asin", 1, ':', l_asin },
    { "atan", 1, ':', l_atan },
    { "atan2", 2, ':', l_atan2 },
    { "ceil", 1, ':', l_ceil },
    { "cos", 1, ':', l_cos },
    { "exp", 1, ':', l_exp },
    { "floor", 1, ':', l_floor },
    { "if", 3, ':', l_if },
    { "log", 1, ':', l_log },
    { "log10", 1, ':', l_log10 },
    { "rand", 1, ':', l_rand },
    { "select", 1, ':', l_select },
    { "sin", 1, ':', l_sin },
    { "sqrt", 1, ':', l_sqrt },
    { "tan", 1, ':', l_tan },
};

static int  libsize = 16;

#else
			/* functions must be listed alphabetically */
static LIBR  library[MAXLIB] = {
    { "ceil", 1, ':', l_ceil },
    { "floor", 1, ':', l_floor },
    { "if", 3, ':', l_if },
    { "rand", 1, ':', l_rand },
    { "select", 1, ':', l_select },
};

static int  libsize = 5;

#endif

extern char  *savestr(), *emalloc();

extern LIBR  *liblookup();

extern VARDEF  *argf();

#ifdef  VARIABLE
#define  resolve(ep)	((ep)->type==VAR?(ep)->v.ln:argf((ep)->v.chan))
#else
#define  resolve(ep)	((ep)->v.ln)
#define varlookup(name)	NULL
#endif


int
fundefined(fname)		/* return # of arguments for function */
char  *fname;
{
    LIBR  *lp;
    register VARDEF  *vp;

    if ((vp = varlookup(fname)) == NULL || vp->def == NULL
		|| vp->def->v.kid->type != FUNC)
	if ((lp = liblookup(fname)) == NULL)
	    return(0);
	else
	    return(lp->nargs);
    else
	return(nekids(vp->def->v.kid) - 1);
}


double
funvalue(fname, n, a)		/* return a function value to the user */
char  *fname;
int  n;
double  *a;
{
    ACTIVATION  act;
    register VARDEF  *vp;
    double  rval;
					/* push environment */
    act.name = fname;
    act.prev = curact;
    act.ap = a;
    if (n >= AFLAGSIZ)
	act.an = ~0;
    else
	act.an = (1L<<n)-1;
    act.fun = NULL;
    curact = &act;

    if ((vp = varlookup(fname)) == NULL || vp->def == NULL
		|| vp->def->v.kid->type != FUNC)
	rval = libfunc(fname, vp);
    else
	rval = evalue(vp->def->v.kid->sibling);

    curact = act.prev;			/* pop environment */
    return(rval);
}


funset(fname, nargs, assign, fptr)	/* set a library function */
char  *fname;
int  nargs;
int  assign;
double  (*fptr)();
{
    register LIBR  *lp;

    if ((lp = liblookup(fname)) == NULL) {
	if (libsize >= MAXLIB) {
	    eputs("Too many library functons!\n");
	    quit(1);
	}
	for (lp = &library[libsize]; lp > library; lp--)
	    if (strcmp(lp[-1].fname, fname) > 0) {
		lp[0].fname = lp[-1].fname;
		lp[0].nargs = lp[-1].nargs;
		lp[0].atyp = lp[-1].atyp;
		lp[0].f = lp[-1].f;
	    } else
		break;
	libsize++;
    }
    lp[0].fname = fname;		/* must be static! */
    lp[0].nargs = nargs;
    lp[0].atyp = assign;
    lp[0].f = fptr;
}


int
nargum()			/* return number of available arguments */
{
    register int  n;

    if (curact == NULL)
	return(0);
    if (curact->fun == NULL) {
	for (n = 0; (1L<<n) & curact->an; n++)
	    ;
	return(n);
    }
    return(nekids(curact->fun) - 1);
}


double
argument(n)			/* return nth argument for active function */
register int  n;
{
    register ACTIVATION  *actp = curact;
    register EPNODE  *ep;
    double  aval;

    if (actp == NULL || --n < 0) {
	eputs("Bad call to argument!\n");
	quit(1);
    }
						/* already computed? */
    if (n < AFLAGSIZ && 1L<<n & actp->an)
	return(actp->ap[n]);

    if (actp->fun == NULL || (ep = ekid(actp->fun, n+1)) == NULL) {
	eputs(actp->name);
	eputs(": too few arguments\n");
	quit(1);
    }
    curact = actp->prev;			/* pop environment */
    aval = evalue(ep);				/* compute argument */
    curact = actp;				/* push back environment */
    if (n < ALISTSIZ) {				/* save value */
	actp->ap[n] = aval;
	actp->an |= 1L<<n;
    }
    return(aval);
}


#ifdef  VARIABLE
VARDEF *
argf(n)				/* return function def for nth argument */
int  n;
{
    register ACTIVATION  *actp;
    register EPNODE  *ep;

    for (actp = curact; actp != NULL; actp = actp->prev) {

	if (n <= 0)
	    break;

	if (actp->fun == NULL)
	    goto badarg;

	if ((ep = ekid(actp->fun, n)) == NULL) {
	    eputs(actp->name);
	    eputs(": too few arguments\n");
	    quit(1);
	}
	if (ep->type == VAR)
	    return(ep->v.ln);			/* found it */

	if (ep->type != ARG)
	    goto badarg;

	n = ep->v.chan;				/* try previous context */
    }
    eputs("Bad call to argf!\n");
    quit(1);

badarg:
    eputs(actp->name);
    eputs(": argument not a function\n");
    quit(1);
}


char *
argfun(n)			/* return function name for nth argument */
int  n;
{
    return(argf(n)->name);
}
#endif


double
efunc(ep)				/* evaluate a function */
register EPNODE  *ep;
{
    ACTIVATION  act;
    double  alist[ALISTSIZ];
    double  rval;
    register VARDEF  *dp;
					/* push environment */
    dp = resolve(ep->v.kid);
    act.name = dp->name;
    act.prev = curact;
    act.ap = alist;
    act.an = 0;
    act.fun = ep;
    curact = &act;

    if (dp->def == NULL || dp->def->v.kid->type != FUNC)
	rval = libfunc(act.name, dp);
    else
	rval = evalue(dp->def->v.kid->sibling);
    
    curact = act.prev;			/* pop environment */
    return(rval);
}


LIBR *
liblookup(fname)		/* look up a library function */
char  *fname;
{
    int  upper, lower;
    register int  cm, i;

    lower = 0;
    upper = cm = libsize;

    while ((i = (lower + upper) >> 1) != cm) {
	cm = strcmp(fname, library[i].fname);
	if (cm > 0)
	    lower = i;
	else if (cm < 0)
	    upper = i;
	else
	    return(&library[i]);
	cm = i;
    }
    return(NULL);
}


#ifndef  VARIABLE
VARDEF *
varinsert(vname)		/* dummy variable insert */
char  *vname;
{
    register VARDEF  *vp;

    vp = (VARDEF *)emalloc(sizeof(VARDEF));
    vp->name = savestr(vname);
    vp->nlinks = 1;
    vp->def = NULL;
    vp->lib = NULL;
    vp->next = NULL;
    return(vp);
}


varfree(vp)			/* free dummy variable */
register VARDEF  *vp;
{
    freestr(vp->name);
    efree((char *)vp);
}
#endif



/*
 *  The following routines are for internal use:
 */


static double
libfunc(fname, vp)			/* execute library function */
char  *fname;
register VARDEF  *vp;
{
    VARDEF  dumdef;
    double  d;
    int  lasterrno;

    if (vp == NULL) {
	vp = &dumdef;
	vp->lib = NULL;
    }
    if (((vp->lib == NULL || strcmp(fname, vp->lib->fname)) &&
				(vp->lib = liblookup(fname)) == NULL) ||
		vp->lib->f == NULL) {
	eputs(fname);
	eputs(": undefined function\n");
	quit(1);
    }
    lasterrno = errno;
    errno = 0;
    d = (*vp->lib->f)(vp->lib->fname);
#ifdef  IEEE
    if (!finite(d))
	errno = EDOM;
#endif
    if (errno) {
	wputs(fname);
	if (errno == EDOM)
		wputs(": domain error\n");
	else if (errno == ERANGE)
		wputs(": range error\n");
	else
		wputs(": error in call\n");
	return(0.0);
    }
    errno = lasterrno;
    return(d);
}


/*
 *  Library functions:
 */


static double
l_if()			/* if(cond, then, else) conditional expression */
			/* cond evaluates true if greater than zero */
{
    if (argument(1) > 0.0)
	return(argument(2));
    else
	return(argument(3));
}


static double
l_select()		/* return argument #(A1+1) */
{
	register int  n;

	n = argument(1) + .5;
	if (n == 0)
		return(nargum()-1);
	if (n < 1 || n > nargum()-1) {
		errno = EDOM;
		return(0.0);
	}
	return(argument(n+1));
}


static double
l_rand()		/* random function between 0 and 1 */
{
    extern double  floor();
    double  x;

    x = argument(1);
    x *= 1.0/(1.0 + x*x) + 2.71828182845904;
    x += .785398163397447 - floor(x);
    x = 1e5 / x;
    return(x - floor(x));
}


static double
l_floor()		/* return largest integer not greater than arg1 */
{
    extern double  floor();

    return(floor(argument(1)));
}


static double
l_ceil()		/* return smallest integer not less than arg1 */
{
    extern double  ceil();

    return(ceil(argument(1)));
}


#ifdef  BIGLIB
static double
l_sqrt()
{
    extern double  sqrt();

    return(sqrt(argument(1)));
}


static double
l_sin()
{
    extern double  sin();

    return(sin(argument(1)));
}


static double
l_cos()
{
    extern double  cos();

    return(cos(argument(1)));
}


static double
l_tan()
{
    extern double  tan();

    return(tan(argument(1)));
}


static double
l_asin()
{
    extern double  asin();

    return(asin(argument(1)));
}


static double
l_acos()
{
    extern double  acos();

    return(acos(argument(1)));
}


static double
l_atan()
{
    extern double  atan();

    return(atan(argument(1)));
}


static double
l_atan2()
{
    extern double  atan2();

    return(atan2(argument(1), argument(2)));
}


static double
l_exp()
{
    extern double  exp();

    return(exp(argument(1)));
}


static double
l_log()
{
    extern double  log();

    return(log(argument(1)));
}


static double
l_log10()
{
    extern double  log10();

    return(log10(argument(1)));
}
#endif
