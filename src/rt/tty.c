/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  tty.c - i/o for terminal drivers.
 *
 *     4/24/87
 */

#include  <stdio.h>
#include  <signal.h>
#include  <fcntl.h>
#include  <sys/ioctl.h>

#include  "driver.h"

/*
 *  Drivers using these routines must call getch()
 *  for input from stdin.
 */

struct sgttyb  ttymode;			/* original tty modes */

int  ttyfd;				/* tty file descriptor */

struct driver  *ttydev;			/* tty device */


ttyset(dev, fd)				/* set up raw tty device */
struct driver  *dev;
int  fd;
{
	int  ttyin(), newinp();
	struct sgttyb  flags;

	if (!isatty(fd))
		return(-1);

	ttyfd = fd;
	ioctl(ttyfd, TIOCGETP, &ttymode);
	ioctl(ttyfd, TIOCGETP, &flags);
	flags.sg_flags |= RAW;			/* also turns output */
	flags.sg_flags &= ~ECHO;		/* processing off */
	ioctl(ttyfd, TIOCSETP, &flags);
						/* install input handler */
	ttydev = dev;
	ttydev->comin = ttyin;
	ttydev->inpready = 0;
	signal(SIGIO, newinp);
	fcntl(ttyfd, F_SETFL, FASYNC);
	return(0);
}


ttydone()				/* restore tty modes */
{
	fcntl(ttyfd, F_SETFL, 0);
	signal(SIGIO, SIG_DFL);
	ioctl(ttyfd, TIOCSETP, &ttymode);
	ttydev->comin = NULL;
	ttydev->inpready = 0;
	ttydev = NULL;
}


int
getch()					/* get a character in raw mode */
{
	register int  c;

	fcntl(ttyfd, F_SETFL, 0);		/* allow read to block */
	if (ttydev->inpready > 0)
		ttydev->inpready--;
	c = getchar();
	fcntl(ttyfd, F_SETFL, FASYNC);
	return(c);
}


static
ttyin(buf)				/* read a line in raw mode */
char  *buf;
{
	int  getch();

	editline(buf, getch, ttydev->comout);
}


static
newinp()				/* new input */
{
	ttydev->inpready++;
}
