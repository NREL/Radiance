/* Copyright (c) 1992 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  calcomp.h - header file for expression parser.
 *
 */
				/* EPNODE types */
#define	 VAR		1
#define	 NUM		2
#define	 UMINUS		3
#define	 CHAN		4
#define	 FUNC		5
#define	 ARG		6
#define	 TICK		7
#define	 SYM		8
				/* also: '+', '-', '*', '/', '^', '=', ':' */

typedef struct {
    char  *fname;		/* function name */
    short  nargs;		/* # of required arguments */
    short  atyp;		/* assignment type (':' or '=') */
    double  (*f)();		/* pointer to function */
}  LIBR;		/* a library function */

typedef struct epnode {
    int	 type;			/* node type */
    struct epnode  *sibling;	/* next child this level */
    union {
	struct epnode  *kid;	/* first child */
	double	num;		/* number */
	char  *name;		/* symbol name */
	int  chan;		/* channel number */
	unsigned long  tick;	/* timestamp */
	struct vardef {
	    char  *name;		/* variable name */
	    int	 nlinks;		/* number of references */
	    struct epnode  *def;	/* definition */
	    LIBR  *lib;			/* library definition */
	    struct vardef  *next;	/* next in hash list */
	}  *ln;			/* link */
    } v;		/* value */
}  EPNODE;	/* an expression node */

typedef struct vardef  VARDEF;	/* a variable definition */

#define	 MAXWORD	127		/* maximum word/id length */
#define	 CNTXMARK	'`'		/* context mark */

#define	 isid(c)	(isalnum(c) || (c) == '_' || \
			(c) == '.' || (c) == CNTXMARK)

extern double  eval(), varvalue(), chanvalue(), funvalue();
extern double  argument(), getnum();
extern double  (*eoper[])();
extern int  getinum();
extern char  *getname(), *qualname(), *argfun();
extern char  *setcontext(), *pushcontext(), *popcontext();
extern EPNODE  *eparse(), *ekid(), *dlookup(), *dpop(), *dfirst(), *dnext();
extern EPNODE  *getdefn(), *getchan();
extern EPNODE  *getE1(), *getE2(), *getE3(), *getE4(), *getE5(), *rconst();
extern VARDEF  *varinsert(), *varlookup(), *argf();
extern LIBR  *liblookup();
extern unsigned long  eclock;
extern int  nextc;

#define	 evalue(ep)	(*eoper[(ep)->type])(ep)
