/* Copyright (c) 1995 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  Header for programs that load variable files.
 */

typedef struct {
	char	*name;		/* variable name */
	short	nick;		/* # characters required for nickname */
	short	nass;		/* # assignments made */
	char	*value;		/* assigned value(s) */
	int	(*fixval)();	/* assignment checking function */
} VARIABLE;		/* a variable-value pair */

/**** The following variables should be declared by calling program ****/

extern int	NVARS;		/* total number of variables */

extern VARIABLE	vv[];		/* variable-value pairs */

extern char	*progname;	/* global argv[0] from main */

extern int	nowarn;		/* global boolean to turn warnings off */

/**** The rest is declared in loadvars.c ****/

extern int	onevalue(), catvalues(), boolvalue(),
		qualvalue(), fltvalue(), intvalue();

extern VARIABLE	*matchvar();
extern char	*nvalue();

#define UPPER(c)	((c)&~0x20)	/* ASCII trick */

#define vnam(vc)	(vv[vc].name)
#define vdef(vc)	(vv[vc].nass)
#define vval(vc)	(vv[vc].value)
#define vint(vc)	atoi(vval(vc))
#define vlet(vc)	UPPER(vval(vc)[0])
#define vscale		vlet
#define vbool(vc)	(vlet(vc)=='T')

#define HIGH		'H'
#define MEDIUM		'M'
#define LOW		'L'
