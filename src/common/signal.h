/* Copyright (c) 1992 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Dummy signal definitions for non-UNIX systems
 */

#ifdef NIX

#define NSIG	2

#define SIGHUP	1	/* hangup */
#define SIGINT	1	/* interrupt */
#define SIGQUIT 1	/* quit */
#define SIGILL	1	/* illegal instruction (not reset when caught) */
#define SIGTRAP 1	/* trace trap (not reset when caught) */
#define SIGIOT	1	/* IOT instruction */
#define SIGEMT	1	/* EMT instruction */
#define SIGFPE	1	/* floating point exception */
#define SIGKILL 1	/* kill (cannot be caught or ignored) */
#define SIGBUS	1	/* bus error */
#define SIGSEGV 1	/* segmentation violation */
#define SIGSYS	1	/* bad argument to system call */
#define SIGPIPE 1	/* write on a pipe with no one to read it */
#define SIGALRM 1	/* alarm clock */
#define SIGTERM 1	/* software termination signal from kill */
#define SIGURG	1	/* urgent condition on IO channel */
#define SIGCONT 1	/* continue a stopped process */
#define SIGCHLD 1	/* to parent on child stop or exit */
#define SIGTTIN 1	/* to readers pgrp upon background tty read */
#define SIGTTOU 1	/* like TTIN for output if (tp->t_local&LTOSTOP) */
#define SIGIO	1	/* input/output possible signal */
#define SIGLOST 1	/* resource lost (eg, record-lock lost) */

#define BADSIG		(int (*)())-1
#define SIG_DFL		(int (*)())0
#define SIG_IGN		(int (*)())1

/*
 * Macro for converting signal number to a mask suitable for sigblock().
 */
#define sigmask(m)	(1 << ((m)-1))

#define signal(n,f)	SIG_DFL

#define sigblock(m)	0

#define alarm(t)	0

#else

#include "/usr/include/signal.h"

#endif
