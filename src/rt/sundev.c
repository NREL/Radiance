/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  sundev.c - standalone driver for Sunwindows.
 *
 *     10/3/88
 */

#include  "standard.h"
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

#define CWIDTH		1100		/* starting canvas width */
#define CHEIGHT		800		/* starting canvas height */

static int  sun_close(), sun_clear(), sun_paintr(), sun_getcur(),
		sun_comout(), sun_comin();

static struct driver  sun_driver = {
	sun_close, sun_clear, sun_paintr,
	sun_getcur, sun_comout, sun_comin,
	NULL, 1.0, CWIDTH, CHEIGHT
};

static FILE  *ttyin;

static Frame  frame = 0;
static Tty  tty = 0;
static Canvas  canvas = 0;

static int  xres = 0, yres = 0;

extern char  *progname;


struct driver *
sun_init(name, id)		/* initialize SunView */
char  *name, *id;
{
	extern Notify_value	newinput(), my_destroy_func();
	extern int	canvas_resize();
	extern char	*getenv();
	char	*gv;
	char	*ttyargv[3], arg1[16];
	int	pd[2];
	int	com;

	frame = window_create(0, FRAME,
			FRAME_LABEL, id==NULL ? name : id,
			FRAME_ICON, &sun_icon,
			WIN_X, 0, WIN_Y, 0,
			0);
	if (frame == 0) {
		eputs("cannot create frame\n");
		return(NULL);
	}
	if (pipe(pd) == -1) {
		eputs("cannot create pipe\n");
		return(NULL);
	}
	sprintf(arg1, "%d", pd[1]);
	ttyargv[0] = TTYPROG;
	ttyargv[1] = arg1;
	ttyargv[2] = NULL;
#ifdef BSD
	fcntl(pd[0], F_SETFD, 1);
#endif
	tty = window_create(frame, TTY,
			TTY_ARGV, ttyargv,
			WIN_ROWS, COMLH,
			TTY_QUIT_ON_CHILD_DEATH, TRUE,
			0);
	if (tty == 0) {
		eputs("cannot create tty subwindow\n");
		return(NULL);
	}
	close(pd[1]);
	if ((ttyin = fdopen(pd[0], "r")) == NULL) {
		eputs("cannot open tty\n");
		return(NULL);
	}
	notify_set_input_func(sun_init, newinput, pd[0]);
	canvas = window_create(frame, CANVAS,
			CANVAS_RETAINED, FALSE,
			CANVAS_RESIZE_PROC, canvas_resize,
			WIN_INPUT_DESIGNEE, window_get(tty,WIN_DEVICE_NUMBER),
			WIN_CONSUME_PICK_EVENTS, WIN_NO_EVENTS,
				MS_LEFT, MS_MIDDLE, MS_RIGHT, 0,
			0);
	if (canvas == 0) {
		eputs("cannot create canvas\n");
		return(NULL);
	}
	if (getmap() < 0) {
		eputs("not a color screen\n");
		return(NULL);
	}
	if ((gv = getenv("DISPLAY_GAMMA")) != NULL)
		make_gmap(atof(gv));
	else
		make_gmap(GAMMA);
	window_set(canvas, CANVAS_RETAINED, TRUE, 0);

	notify_interpose_destroy_func(frame, my_destroy_func);
	sun_driver.inpready = 0;
	return(&sun_driver);
}


static
sun_close()				/* all done */
{
	Frame	fr;

	fr = frame;
	frame = 0;		/* death cry */
	if (fr != 0) {
		window_set(fr, FRAME_NO_CONFIRM, TRUE, 0);
		window_destroy(fr);
	}
}


static Notify_value
my_destroy_func(fr, st)			/* say bye-bye */
Frame	fr;
Destroy_status	st;
{
	if (st != DESTROY_CHECKING && frame != 0)
		quit(1);		/* how rude! */
	return(notify_next_destroy_func(fr, st));
}


static Notify_value
newinput(cid, fd)			/* register new input */
int  (*cid)();
int  fd;
{
	notify_set_input_func(sun_init, NOTIFY_FUNC_NULL, fd);
	getc(ttyin);
	sun_driver.inpready++;
	return(NOTIFY_DONE);
}


static
canvas_resize(canvas, width, height)	/* canvas being resized */
Canvas	canvas;
int	width, height;
{
	if (width == xres && height == yres)
		return;
	sun_driver.xsiz = width;
	sun_driver.ysiz = height;
	tocombuf("new\n", &sun_driver);
}


static
sun_clear(nwidth, nheight)		/* clear our canvas */
int  nwidth, nheight;
{
	register Pixwin  *pw;

	pw = canvas_pixwin(canvas);
	if (nwidth != xres || nheight != yres) {
		xres = nwidth;
		yres = nheight;
		window_set(frame, WIN_SHOW, FALSE, 0);
		window_set(canvas, WIN_WIDTH, nwidth, WIN_HEIGHT, nheight, 0);
		window_fit(frame);
		window_set(frame, WIN_SHOW, TRUE, 0);
		notify_do_dispatch();
	}
	pw_writebackground(pw, 0, 0, xres, xres, PIX_SRC);
	new_ctab(NCOLORS);
}


static
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


static
sun_comin(buf, prompt)		/* input a string from the command line */
char  *buf, *prompt;
{
	extern Notify_value  newinput();
						/* check command buffer */
	if (prompt != NULL)
		if (fromcombuf(buf, &sun_driver))
			return;
		else
			sun_comout(prompt);
						/* await signal */
	if (sun_driver.inpready <= 0)
		newinput(sun_init, fileno(ttyin));
						/* echo characters */
	do {
		mygets(buf, ttyin);
		sun_comout(buf);
	} while (buf[0]);
						/* get result */
	mygets(buf, ttyin);

	if (sun_driver.inpready > 0)
		sun_driver.inpready--;
						/* reinstall handler */
	notify_set_input_func(sun_init, newinput, fileno(ttyin));
}


static int
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
		eputs("window event read error\n");
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


static
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


static
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


static
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
