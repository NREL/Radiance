/* Copyright (c) 1988 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  suncom.c - program to read and edit raw tty input.
 *
 *	10/5/88
 */

#include  <stdio.h>
#include  <signal.h>
#include  <sys/ioctl.h>

#define INITSTR		""		/* TTY initialization string */

struct sgttyb  ttymode;


main(argc, argv)		/* called with pid to signal and pipe fd */
int	argc;
char	*argv[];
{
	int  sigpid;
	int  outfd;

	if (argc != 3)
		exit(1);
	sigpid = atoi(argv[1]);
	outfd = atoi(argv[2]);

	fputs(INITSTR, stdout);
	fflush(stdout);
						/* reassign stdout */
	if (outfd != fileno(stdout)) {
		dup2(outfd, fileno(stdout));
		close(outfd);
	}
						/* set tty modes */
	if (!isatty(0))
		exit(1);
	ioctl(0, TIOCGETP, &ttymode);
	ttymode.sg_flags |= RAW;		/* also turns output */
	ttymode.sg_flags &= ~ECHO;		/* processing off */
	ioctl(0, TIOCSETP, &ttymode);
						/* read lines from input */
	for ( ; ; ) {
		ungetc(getc(stdin), stdin);	/* notify sigpid */
		kill(sigpid, SIGIO);
		sendline();
	}
}


int
getch()					/* get a character in raw mode */
{
	return(getc(stdin));
}


sends(s)				/* send a string to stdout */
register char  *s;
{
	do
		putc(*s, stdout);
	while (*s++);
	fflush(stdout);
}


sendline()				/* read a line in raw mode */
{
	char  buf[256];

	editline(buf, getch, sends);
	putc('\0', stdout);
	sends(buf);
}
