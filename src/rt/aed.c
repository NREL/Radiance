/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  aed.c - driver for AED 512 terminal.
 *
 *     2/2/87
 */

#include  <stdio.h>

#include  "driver.h"

#include  "color.h"


	/* AED command characters */

#define AEDFMT	"1888N"	/* Format string to send to AED */
#define CSTAT	0100	/* Console status: lower case */
#define SCT	75	/* Set color lookup table */
#define SEC	67	/* Set color for vector drawing */
#define MOV	81	/* Set CAP (current access pointer) to x, y */
#define OPT	40	/* Miscellaneous terminal control */
#define SEN	71	/* Set encoding types */
#define RST	48	/* Reset terminal */
#define FFD	12	/* Clear screen */
#define EJC	85	/* Enable Joystick cursor positioning */
#define DJC	100	/* Disable Joystick cursor positioning */
#define SCC	99	/* Set Cursor Colors */
#define SCP	93	/* Set Cursor Parameters */
#define DFR	111	/* Draw Filled Rectangle */
#define RCP	106	/* Read Cursor Position */
#define ESC	27	/* Escape starts a command sequence */
#define SCS	96	/* Set Console Status */
#define END	1	/* Ctrl-A used to terminate command mode */

#define BLK	0	/* color table entry for black */
#define WHT	7	/* color table entry for white */

#define command(c)	(putc(ESC, stdout), putc(c, stdout))
#define byte(b)		putc(b, stdout)
#define string(s)	fputs(s, stdout)
#define flush()		fflush(stdout)

#define  GAMMA		2.0		/* exponent for color correction */

#define  MINCOLOR	8		/* start of device color table */

#define  NCOLORS	241		/* our color table size (prime) */

#define  hashcolr(c)	((67*(c)[RED]+59*(c)[GRN]+71*(c)[BLU])%NCOLORS)

#define  NCOLS		512		/* maximum # columns for output */
#define  NROWS		512-COMHT	/* maximum # rows for output */
#define  COMHT		16		/* height of command line */
#define  COMCW		63		/* maximum chars on command line */

static COLR  colrmap[256];		/* our color map */

static COLR  colrtbl[NCOLORS];		/* our color table */

static int  colres = 64;		/* color resolution */

int  aed_close(), aed_clear(), aed_paintr(),
		aed_getcur(), aed_comout(), aed_errout();

static struct driver  aed_driver = {
	aed_close, aed_clear, aed_paintr, aed_getcur,
	aed_comout, NULL,
	NCOLS, NROWS
};


struct driver *
aed_init(name)				/* open AED */
char  *name;
{
	if (ttyset(&aed_driver, fileno(stdin)) < 0) {	/* set tty driver */
		stderr_v("cannot access terminal\n");
		return(NULL);
	}
	command(RST);					/* reset AED */
	longwait(2);
	command(OPT);
	byte(6); byte(1);				/* AED command set */
	command(SEN);
	string(AEDFMT);
	command(SCS);					/* set console status */
	byte(CSTAT);
	command(SCC);					/* set cursor type */
	byte(BLK); byte(WHT); byte(15);
	command(SCP);
	byte('+'); byte(0); byte(1);
	errvec = aed_errout;				/* set error vector */
	cmdvec = aed_errout;
	if (wrnvec != NULL)
		wrnvec = aed_errout;
	return(&aed_driver);
}


static
aed_close()					/* close AED */
{
	errvec = stderr_v;			/* reset error vector */
	cmdvec = NULL;
	if (wrnvec != NULL)
		wrnvec = stderr_v;
	aedsetcap(0, 0);			/* go to bottom */
	command(SEC);
	byte(WHT);				/* white text */
	byte(END);
	byte('\n');				/* scroll */
	flush();
	ttydone();
}


static
aed_clear(x, y)					/* clear AED */
int  x, y;
{
	command(FFD);
	flush();
	makemap();				/* init color map */
}


static
aed_paintr(col, xmin, ymin, xmax, ymax)		/* paint a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	int  ndx;

	if ((ndx = colindex(col)) < 0) {		/* full table? */
		colres >>= 1;
		redraw();
		return;
	}
	command(SEC);					/* draw rectangle */
	byte(ndx+MINCOLOR);
	aedsetcap(xmin, ymin+COMHT);
	command(DFR);
	aedcoord(xmax-1, ymax+(-1+COMHT));
	flush();
}


static int
aed_getcur(xp, yp)			/* get cursor position */
int  *xp, *yp;
{
	int  com;

	command(EJC);			/* enable cursor */
	com = getch();			/* get key */
	command(DJC);			/* disable cursor */
	aedgetcap(xp, yp);		/* get cursor position */
	*yp -= COMHT;			/* convert coordinates */
	return(com);			/* return key hit */
}


static
aed_comout(out)				/* output to command line */
register char  *out;
{
	static int  newl = 1;
	static int  inmsg = 0;

	while (*out) {
		if (newl) {			/* new line */
			command(SEC);
			byte(BLK);
			aedsetcap(0, 0);
			command(DFR);
			aedcoord(NCOLS-1, COMHT-1);
			aedsetcap(1, 4);
			command(SEC);		/* white text */
			byte(WHT);
			byte(END);
			newl = 0;
		}
		switch (*out) {
		case '\n':			/* end of line */
			newl = 1;
		/* fall through */
		case '\r':
			inmsg = 0;
			byte('\r');
			break;
		case '\b':			/* back space */
			if (inmsg > 0 && --inmsg < COMCW)
				byte('\b');
			break;
		default:			/* character */
			if (inmsg++ < COMCW)
				byte(*out);
			break;
		}
		out++;
	}
	flush();
}


static
aed_errout(msg)				/* print an error message */
char  *msg;
{
	aed_comout(msg);
	if (*msg && msg[strlen(msg)-1] == '\n')
		longwait(2);
}


/*
 * aedsetcap - sets AED's current access pointer to (x, y).
 */

static
aedsetcap(x, y)
register int  x, y;
{
	command(MOV);
	aedcoord(x, y);
}

/*
 * aedcoord - puts out an (x, y) coordinate in AED 8 bit format.
 */

static
aedcoord(x, y)
register int  x, y;
{
	byte(((x >> 4) & 0x30) | ((y >> 8) & 0x3));
	byte(x & 0xff);
	byte(y & 0xff);
}


static
aedgetcap(xp, yp)		/* get cursor postion */
int  *xp, *yp;
{
	register int  c;
				/* get cursor postition */
	command(RCP);
	flush();
	c = getch();
	*xp = (c & 0x30) << 4;
	*yp = (c & 0x3) << 8;
	*xp |= getch();
	*yp |= getch();
}


static int
colindex(col)			/* return table index for color */
COLOR  col;
{
	static COLR  clr;
	int  hval;
	register int  ndx, i;
	
	mapcolor(clr, col);

	hval = ndx = hashcolr(clr);
	
	for (i = 1; i < NCOLORS; i++) {
		if (colrtbl[ndx][EXP] == 0) {
			colrtbl[ndx][RED] = clr[RED];
			colrtbl[ndx][GRN] = clr[GRN];
			colrtbl[ndx][BLU] = clr[BLU];
			colrtbl[ndx][EXP] = COLXS;
			newcolr(ndx, clr);
			return(ndx);
		}
		if (		colrtbl[ndx][RED] == clr[RED] &&
				colrtbl[ndx][GRN] == clr[GRN] &&
				colrtbl[ndx][BLU] == clr[BLU]	)
			return(ndx);
		ndx = (hval + i*i) % NCOLORS;
	}
	return(-1);
}


static
newcolr(index, clr)		/* enter a color into our table */
int  index;
COLR  clr;
{
	command(SCT);
	byte(index+MINCOLOR);
	byte(1);
	byte(clr[RED]);
	byte(clr[GRN]);
	byte(clr[BLU]);
	flush();
}


static
mapcolor(clr, col)			/* map to our color space */
COLR  clr;
COLOR  col;
{
	register int  i, p;
					/* compute color table value */
	for (i = 0; i < 3; i++) {
		p = colval(col,i) * 256.0;
		if (p < 0) p = 0;
		else if (p >= 256) p = 255;
		clr[i] = colrmap[p][i];
	}
	clr[EXP] = COLXS;
}


static
makemap()				/* initialize the color map */
{
	double  pow();
	int  val;
	register int  i;

	for (i = 0; i < 256; i++) {
		val = pow(i/256.0, 1.0/GAMMA) * colres;
		val = (val*256 + 128) / colres;
		colrmap[i][RED] = colrmap[i][GRN] = colrmap[i][BLU] = val;
		colrmap[i][EXP] = COLXS;
	}
	for (i = 0; i < NCOLORS; i++)
		colrtbl[i][EXP] = 0;
}


static
longwait(t)		/* longer wait */
int  t;
{
	flush();
	sleep(t);
}
