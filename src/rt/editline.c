/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  editline.c - routine for editing raw input for rview.
 *
 *	10/5/88
 */

#include  <ctype.h>


editline(buf, c_get, s_put, c_erase, c_kill)	/* edit input line */
char  *buf;
int  (*c_get)(), (*s_put)();
int  c_erase, c_kill;
{
	static char  erases[] = "\b \b";
	static char  obuf[4];
	register int  i;
	register int  c;
	
	i = 0;
	while ((c = (*c_get)()&0177) != '\n' && c != '\r')
		if (c == c_erase) {		/* single char erase */
			if (i > 0) {
				(*s_put)(erases);
				--i;
			}
		} else if (c == c_kill) {	/* kill line */
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
