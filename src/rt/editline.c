/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  editline.c - routine for editing raw input for rview.
 *
 *	10/5/88
 */

#define iscntrl(c)	((c) < ' ')
#define isblank(c)	((c) == ' ')
#define iserase(c)	((c) == '\b' || (c) == 127)
#define iswerase(c)	((c) == 'W'-'@')
#define iskill(c)	((c) == 'U'-'@' || (c) == 'X'-'@')


editline(buf, c_get, s_put)	/* edit input line */
char  *buf;
int  (*c_get)(), (*s_put)();
{
	static char  erases[] = "\b \b";
	static char  obuf[4];
	register int  i;
	register int  c;
	
	i = 0;
	while ((c = (*c_get)()&0177) != '\n' && c != '\r')
		if (iserase(c)) {		/* single char erase */
			if (i > 0) {
				(*s_put)(erases);
				--i;
			}
		} else if (iswerase(c)) {	/* word erase */
			while (i > 0 && isblank(buf[i-1])) {
				(*s_put)(erases);
				--i;
			}
			while (i > 0 && !isblank(buf[i-1])) {
				(*s_put)(erases);
				--i;
			}
		} else if (iskill(c)) {		/* kill line */
			while (i > 0) {
				(*s_put)(erases);
				--i;
			}
		} else if (iscntrl(c)) {		/* control char */
			i = 0;
			buf[i++] = c;
			obuf[0] = '^'; obuf[1] = c|0100; obuf[2] = '\0';
			(*s_put)(obuf);
			break;
		} else {				/* regular char */
			buf[i++] = c;
			obuf[0] = c; obuf[1] = '\0';
			(*s_put)(obuf);
		}
	buf[i] = '\0';
	(*s_put)("\n");
}


#include  "driver.h"

static char  mybuf[512];


tocombuf(b, d)				/* add command(s) to my buffer */
register char  *b;
register struct driver  *d;
{
	register char  *cp;

	for (cp = mybuf; *cp; cp++)
		;
	while (*cp++ = *b)
		if (*b++ == '\n')
			d->inpready++;
}


fromcombuf(b, d)			/* get command from my buffer */
char  *b;
struct driver  *d;
{
	register char	*cp;
						/* get next command */
	for (cp = mybuf; *cp != '\n'; cp++)
		if (!*cp)
			return(0);
	*cp++ = '\0';
#ifdef DEBUG
	(*d->comout)(mybuf);			/* echo my command */
	(*d->comout)("\n");
#endif
						/* send it as reply */
	strcpy(b, mybuf);
	d->inpready--;
						/* get next command */
	strcpy(mybuf, cp);
	return(1);
}
