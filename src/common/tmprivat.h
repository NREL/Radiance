/* Copyright (c) 1997 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Private header file for tone mapping routines.
 */

#undef	NOPROTO
#define	NOPROTO		1
#include	"tonemap.h"
				/* minimum values and defaults */
#define	MINGAM		0.75
#define DEFGAM		2.2
#define	MINLDDYN	2.
#define DEFLDDYN	32.
#define	MINLDMAX	1.
#define	DEFLDMAX	100.

				/* private flags */
#define	TM_F_INITED	010000		/* initialized flag */
#define	TM_F_NEEDMAT	020000		/* need matrix conversion */

#define BRT2SCALE	((int)(M_LN2*TM_BRTSCALE+.5))

#define HISTEP		8		/* steps in BRTSCALE for each bin */

#define MINBRT		(-16*TM_BRTSCALE)	/* minimum usable brightness */
#define MINLUM		(1.125352e-7)		/* tmLuminance(MINBRT) */

#define LMESLOWER	(5.62e-3)		/* lower mesopic limit */
#define BMESLOWER	((int)(-5.18*TM_BRTSCALE-.5))
#define	LMESUPPER	(5.62)			/* upper mesopic limit */
#define BMESUPPER	((int)(1.73*TM_BRTSCALE+.5))

#ifndef	int4
#define	int4		int			/* assume 32-bit integers */
#endif
						/* approximate scotopic lum. */
#define	SCO_rf		0.062
#define SCO_gf		0.608
#define SCO_bf		0.330
#define scotlum(c)	(SCO_rf*(c)[RED] + SCO_gf*(c)[GRN] + SCO_bf*(c)[BLU])
#define normscot(c)	( (	(int4)(SCO_rf*256.+.5)*(c)[RED] + \
				(int4)(SCO_gf*256.+.5)*(c)[GRN] + \
				(int4)(SCO_bf*256.+.5)*(c)[BLU]	) >> 8 )

#ifndef	malloc
extern char	*malloc(), *calloc();
#endif
extern int	tmErrorReturn();

#define	returnErr(code)	return(tmErrorReturn(funcName,code))
#define returnOK	return(TM_E_OK)

#define	FEQ(a,b)	((a) < (b)+1e-5 && (b) < (a)+1e-5)

#define	PRIMEQ(p1,p2)	(FEQ((p1)[0][0],(p2)[0][0])&&FEQ((p1)[0][1],(p2)[0][2])\
			&&FEQ((p1)[1][0],(p2)[1][0])&&FEQ((p1)[1][1],(p2)[1][2])\
			&&FEQ((p1)[2][0],(p2)[2][0])&&FEQ((p1)[2][1],(p2)[2][2])\
			&&FEQ((p1)[3][0],(p2)[3][0])&&FEQ((p1)[3][1],(p2)[3][2]))
