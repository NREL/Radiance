/* Copyright (c) 1989 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  devmain.c - main for independent drivers.
 *
 *	Redefine your initialization routine to dinit.
 *
 *	10/25/89
 */

#include <stdio.h>

#include <signal.h>

#include "color.h"

#include "driver.h"

int	(*wrnvec)(), (*errvec)(), (*cmdvec)();	/* error vectors, unused */

long	nrays = 0;				/* fake it */

struct driver	*dev = NULL;			/* output device */

FILE	*devin, *devout;			/* communications channels */

int	notified = 0;				/* notified parent of input? */

char	*progname;				/* driver name */

int	r_clear(), r_paintr(), r_getcur(), r_comout(), r_comin();

int	(*dev_func[NREQUESTS])() = {		/* request handlers */
		r_clear, r_paintr,
		r_getcur, r_comout, r_comin
	};


main(argc, argv)		/* set up communications and main loop */
int	argc;
char	*argv[];
{
	extern struct driver	*dinit();
	int  com;
						/* set up I/O */
	progname = argv[0];
	if (argc < 3) {
		stderr_v("arg count\n");
		quit(1);
	}
	devin = fdopen(atoi(argv[1]), "r");
	devout = fdopen(atoi(argv[2]), "w");
	if (devin == NULL || devout == NULL || getw(devin) != COM_SENDM) {
		stderr_v("connection failure\n");
		quit(1);
	}
						/* open device */
	if ((dev = dinit(argv[0], argv[3])) == NULL)
		quit(1);
	putw(COM_RECVM, devout);		/* verify initialization */
	fwrite(&dev->pixaspect, sizeof(dev->pixaspect), 1, devout);
	putw(dev->xsiz, devout);
	putw(dev->ysiz, devout);
	fflush(devout);
						/* loop on requests */
	while ((com = getc(devin)) != EOF) {
		if (com >= NREQUESTS || dev_func[com] == NULL) {
			stderr_v("invalid request\n");
			quit(1);
		}
		(*dev_func[com])();		/* process request */
	}
	quit(0);			/* all done, clean up and exit */
}


quit(code)				/* close device and exit */
int	code;
{
	if (dev != NULL)
		(*dev->close)();
	exit(code);
}


r_clear()				/* clear screen */
{
	int	xres, yres;

	xres = getw(devin);
	yres = getw(devin);
	(*dev->clear)(xres, yres);
}


r_paintr()				/* paint a rectangle */
{
	COLOR	col;
	int	xmin, ymin, xmax, ymax;

	nrays += 5;			/* pretend */
	fread(col, sizeof(COLOR), 1, devin);
	xmin = getw(devin); ymin = getw(devin);
	xmax = getw(devin); ymax = getw(devin);
	(*dev->paintr)(col, xmin, ymin, xmax, ymax);
					/* check for input */
	if (!notified && dev->inpready > 0) {
		notified = 1;
		kill(getppid(), SIGIO);
	}
}


r_getcur()				/* get and return cursor position */
{
	int	c;
	int	x, y;
					/* get it if we can */
	if (dev->getcur == NULL) {
		c = ABORT;
		x = y = 0;
	} else
		c = (*dev->getcur)(&x, &y);
					/* reply */
	putc(COM_GETCUR, devout);
	putc(c, devout);
	putw(x, devout);
	putw(y, devout);
	fflush(devout);
}


r_comout()				/* print string to command line */
{
	char	str[256];

	mygets(str, devin);
	(*dev->comout)(str);
}


r_comin()				/* read string from command line */
{
	char	buf[256];
					/* get string */
	(*dev->comin)(buf);
					/* reply */
	putc(COM_COMIN, devout);
	myputs(buf, devout);
	fflush(devout);
					/* reset notify */
	notified = 0;
}


mygets(s, fp)				/* get string from file (with nul) */
register char	*s;
register FILE	*fp;
{
	register int	c;

	while ((c = getc(fp)) != EOF)
		if ((*s++ = c) == '\0')
			return;
	*s = '\0';
}


myputs(s, fp)				/* put string to file (with nul) */
register char	*s;
register FILE	*fp;
{
	do
		putc(*s, fp);
	while (*s++);
}


stderr_v(s)				/* put string to stderr */
register char  *s;
{
	static int  inline = 0;

	if (!inline++) {
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (*s && s[strlen(s)-1] == '\n') {
		fflush(stderr);
		inline = 0;
	}
}
