/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  vga.c - driver for VGA graphics adaptor under MS-DOS
 */

#include  <stdio.h>

#include  <graph.h>

#include  "driver.h"

#include  "color.h"

#define	 GAMMA		2.2		/* exponent for color correction */


int  vga_close(), vga_clear(), vga_paintr(), vga_comout(), vga_comin();

static struct driver  vga_driver = {
	vga_close, vga_clear, vga_paintr, NULL,
	vga_comout, vga_comin, NULL
};

static struct videoconfig  config;


struct driver *
vga_init(name, id)			/* open VGA */
char  *name, *id;
{
	if (_setvideomode(_MRES256COLOR) == 0)
		return(NULL);
	_getvideoconfig(&config);
	_settextwindow(config.numtextrows-2, 1,
			config.numtextrows, config.numtextcols);
	vga_driver.xsiz = config.numxpixels;
	vga_driver.ysiz = config.numypixels*(config.numtextrows-3)
				/config.numtextrows;
	vga_driver.pixaspect = .75*config.numxpixels/config.numypixels;
	_setviewport(0, 0, vga_driver.xsiz, vga_driver.ysiz);
	make_gmap(GAMMA);				/* make color map */
	_remappalette(1, 0x303030L);
	_settextcolor(1);
	errvec = vga_comout;
	cmdvec = vga_comout;
	if (wrnvec != NULL)
		wrnvec = vga_comout;
	return(&vga_driver);
}


static
vga_close()					/* close VGA */
{
	errvec = stderr_v;			/* reset error vector */
	cmdvec = NULL;
	if (wrnvec != NULL)
		wrnvec = stderr_v;
	_setvideomode(_DEFAULTMODE);
}


static
vga_clear(x, y)					/* clear VGA */
int  x, y;
{
	_clearscreen(_GCLEARSCREEN);
	new_ctab(config.numcolors-2);		/* init color table */
}


static
vga_paintr(col, xmin, ymin, xmax, ymax)		/* paint a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	extern int  vgacolr();

	_setcolor(get_pixel(col, vgacolr)+2);
	_rectangle(_GFILLINTERIOR, xmin, vga_driver.ysiz-ymax,
			xmax-1, vga_driver.ysiz-1-ymin);
	vga_driver.inpready = kbhit();
}


static
vga_comout(s)				/* put s to text output */
register char  *s;
{
	struct rccoord	tpos;
	char  buf[128];
	register char  *cp;

	for (cp = buf; ; s++) {
		switch (*s) {
		case '\b':
		case '\r':
		case '\0':
			if (cp > buf) {
				*cp = '\0';
				_outtext(cp = buf);
			}
			if (*s == '\0')
				break;
			tpos = _gettextposition();
			_settextposition(tpos.row, *s=='\r' ? 0 : tpos.col-1);
			continue;
		default:
			*cp++ = *s;
			continue;
		}
		return(0);
	}
}


static
vga_comin(buf, prompt)			/* get input line from console */
char  *buf;
char  *prompt;
{
	extern int  getch();

	if (prompt != NULL)
		_outtext(prompt);
	editline(buf, getch, vga_comout);
	vga_driver.inpready = kbhit();
}


static
vgacolr(index, r, g, b)		       /* enter a color into our table */
int  index;
int  r, g, b;
{
	register long  cl;

	cl = (long)b<<14 & 0x3f0000L;
	cl |= g<<6 & 0x3f00;
	cl |= r>>2;
	_remappalette(index+2, cl);
}
