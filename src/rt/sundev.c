/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  sundev.c - standalone driver for Sunwindows.
 *
 *     10/3/88
 */

#include  <stdio.h>
#include  <fcntl.h>
#include  <suntool/sunview.h>
#include  <suntool/canvas.h>
#include  <suntool/tty.h>

#include  "color.h"

#include  "driver.h"

short	icon_image[] = {
#include  "suntools.icon"
};
DEFINE_ICON_FROM_IMAGE(sun_icon, icon_image);

#ifndef TTYPROG
#define TTYPROG		"sun.com"	/* tty program */
#endif

#define GAMMA		2.2		/* exponent for color correction */

#define COMLH		3		/* number of command lines */

#define FIRSTCOLOR	2		/* first usable entry */
#define NCOLORS		251		/* number of usable colors */

int  sun_close(), sun_clear(), sun_paintr(), sun_getcur(),
		sun_comout(), sun_comin();

static struct driver  sun_driver = {
	sun_close, sun_clear, sun_paintr,
	sun_getcur, sun_comout, sun_comin,
	1.0, 1100, 800
};

static FILE  *ttyin;

static Frame  frame = 0;
static Tty  tty = 0;
static Canvas  canvas = 0;

static int  xres = 0, yres = 0;

extern char  *progname;


struct driver *
dinit(name, id)			/* initialize SunView (independent) */
char  *name, *id;
{
	extern Notify_value	my_notice_destroy();
	char	*ttyargv[4], arg1[16], arg2[16];
	int	pd[2];
	int	com;

	frame = window_create(0, FRAME,
			FRAME_LABEL, id==NULL ? name : id,
			FRAME_ICON, &sun_icon,
			WIN_X, 0, WIN_Y, 0,
			0);
	if (frame == 0) {
		stderr_v("cannot create frame\n");
		return(NULL);
	}
	if (pipe(pd) == -1) {
		stderr_v("cannot create pipe\n");
		return(NULL);
	}
	sprintf(arg1, "%d", getppid());
	sprintf(arg2, "%d", pd[1]);
	ttyargv[0] = TTYPROG;
	ttyargv[1] = arg1;
	ttyargv[2] = arg2;
	ttyargv[3] = NULL;
#ifdef BSD
	fcntl(pd[0], F_SETFD, 1);
#endif
	tty = window_create(frame, TTY,
			TTY_ARGV, ttyargv,
			WIN_ROWS, COMLH,
			TTY_QUIT_ON_CHILD_DEATH, TRUE,
			0);
	if (tty == 0) {
		stderr_v("cannot create tty subwindow\n");
		return(NULL);
	}
	close(pd[1]);
	if ((ttyin = fdopen(pd[0], "r")) == NULL) {
		stderr_v("cannot open tty\n");
		return(NULL);
	}
	canvas = window_create(frame, CANVAS,
			CANVAS_RETAINED, FALSE,
			WIN_INPUT_DESIGNEE, window_get(tty,WIN_DEVICE_NUMBER),
			WIN_CONSUME_PICK_EVENTS, WIN_NO_EVENTS,
				MS_LEFT, MS_MIDDLE, MS_RIGHT, 0,
			0);
	if (canvas == 0) {
		stderr_v("cannot create canvas\n");
		return(NULL);
	}
	if (getmap() < 0) {
		stderr_v("not a color screen\n");
		return(NULL);
	}
	make_gmap(GAMMA);
	window_set(canvas, CANVAS_RETAINED, TRUE, 0);

	sun_driver.inpready = 0;
	return(&sun_driver);
}


sun_close()				/* all done */
{
	if (frame != 0) {
		window_set(frame, FRAME_NO_CONFIRM, TRUE, 0);
		window_destroy(frame);
	}
	frame = 0;
}


sun_clear(nwidth, nheight)		/* clear our canvas */
int  nwidth, nheight;
{
	register Pixwin  *pw;

	pw = canvas_pixwin(canvas);
	if (nwidth != xres || nheight != yres) {
		window_set(frame, WIN_SHOW, FALSE, 0);
		window_set(canvas, CANVAS_AUTO_SHRINK, TRUE,
				CANVAS_AUTO_EXPAND, TRUE, 0);
		window_set(canvas, WIN_WIDTH, nwidth, WIN_HEIGHT, nheight, 0);
		window_set(canvas, CANVAS_AUTO_SHRINK, FALSE,
				CANVAS_AUTO_EXPAND, FALSE, 0);
		window_fit(frame);
		window_set(frame, WIN_SHOW, TRUE, 0);
		notify_do_dispatch();
		xres = nwidth;
		yres = nheight;
	}
	pw_writebackground(pw, 0, 0, xres, xres, PIX_SRC);
	new_ctab(NCOLORS);
}


sun_paintr(col, xmin, ymin, xmax, ymax)		/* fill a rectangle */
COLOR  col;
int  xmin, ymin, xmax, ymax;
{
	extern int  newcolr();
	int  pval;
	register Pixwin  *pw;

	pval = get_pixel(col, newcolr) + FIRSTCOLOR;
	pw = canvas_pixwin(canvas);
	pw_rop(pw, xmin, yres-ymax, xmax-xmin, ymax-ymin,
			PIX_SRC|PIX_COLOR(pval), NULL, 0, 0);
}


sun_comin(buf)			/* input a string from the command line */
char  *buf;
{
						/* echo characters */
	do {
		mygets(buf, ttyin);
		sun_comout(buf);
	} while (buf[0]);
						/* get result */
	mygets(buf, ttyin);
}


int
sun_getcur(xpp, ypp)			/* get cursor position */
int  *xpp, *ypp;
{
	Event	ev;
	int	c;

	notify_no_dispatch();			/* allow read to block */
	window_set(canvas, WIN_CONSUME_KBD_EVENT, WIN_ASCII_EVENTS, 0);
again:
	if (window_read_event(canvas, &ev) == -1) {
		notify_perror();
		stderr_v("window event read error\n");
		quit(1);
	}
	c = event_id(&ev);
	switch (c) {
	case MS_LEFT:
		c = MB1;
		break;
	case MS_MIDDLE:
		c = MB2;
		break;
	case MS_RIGHT:
		c = MB3;
		break;
	default:
		if (c < ASCII_FIRST || c > ASCII_LAST)
			goto again;
		break;
	}
	*xpp = event_x(&ev);
	*ypp = yres-1 - event_y(&ev);

	window_set(canvas, WIN_IGNORE_KBD_EVENT, WIN_ASCII_EVENTS, 0);
	notify_do_dispatch();
	return(c);
}


sun_comout(s)			/* put string out to tty subwindow */
register char	*s;
{
	char	buf[256];
	register char	*cp;

	for (cp = buf; *s; s++) {
		if (*s == '\n')
			*cp++ = '\r';
		*cp++ = *s;
	}
					/* must be a single call */
	ttysw_output(tty, buf, cp-buf);
}


getmap()				/* allocate color map segments */
{
	char  cmsname[20];
	unsigned char  red[256], green[256], blue[256];
	register Pixwin  *pw;
	register int  i;

	for (i = 0; i < 256; i++)
		red[i] = green[i] = blue[i] = 128;
	red[0] = green[0] = blue[0] = 255;
	red[1] = green[1] = blue[1] = 0;
	red[255] = green[255] = blue[255] = 0;
	red[254] = green[254] = blue[254] = 255;
	red[253] = green[253] = blue[253] = 0;
						/* name shared segment */
	sprintf(cmsname, "rv%d", getpid());
						/* set canvas */
	pw = canvas_pixwin(canvas);
	if (pw->pw_pixrect->pr_depth < 8)
		return(-1);
	pw_setcmsname(pw, cmsname);
	pw_putcolormap(pw, 0, 256, red, green, blue);
						/* set tty subwindow */
	pw = (Pixwin *)window_get(tty, WIN_PIXWIN);
	pw_setcmsname(pw, cmsname);
	pw_putcolormap(pw, 0, 256, red, green, blue);
						/* set frame */
	pw = (Pixwin *)window_get(frame, WIN_PIXWIN);
	pw_setcmsname(pw, cmsname);
	pw_putcolormap(pw, 0, 256, red, green, blue);

	return(0);
}


newcolr(ndx, r, g, b)		/* enter a color into hardware table */
int  ndx;
unsigned char  r, g, b;
{
	register Pixwin  *pw;
	
	pw = canvas_pixwin(canvas);
	pw_putcolormap(pw, ndx+FIRSTCOLOR, 1,
			&r, &g, &b);
	pw = (Pixwin *)window_get(tty, WIN_PIXWIN);
	pw_putcolormap(pw, ndx+FIRSTCOLOR, 1,
			&r, &g, &b);
}
