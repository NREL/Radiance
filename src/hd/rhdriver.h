/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * Header file for holodeck device driver routines.
 */

#include "view.h"

extern struct driver {
	char	*name;		/* holodeck name or title */
	VIEW	v;		/* base view parameters */
	int	hres, vres;	/* base view resolution */
	int	ifd;		/* input file descriptor (for select) */
} odev;			/* our open device */

extern int	imm_mode;	/* bundles are being delivered immediately */

extern double	eyesepdist;	/* world eye separation distance */

				/* user commands */
#define	DC_SETVIEW	0		/* set the base view */
#define	DC_GETVIEW	1		/* print the current base view */
#define	DC_LASTVIEW	2		/* restore previous view */
#define	DC_PAUSE	3		/* pause the current calculation */
#define	DC_RESUME	4		/* resume the calculation */
#define	DC_REDRAW	5		/* redraw from server */
#define	DC_KILL		6		/* kill rtrace process(es) */
#define	DC_RESTART	7		/* restart rtrace process(es) */
#define DC_CLOBBER	8		/* clobber holodeck file */
#define	DC_QUIT		9		/* quit the program */

#define	DC_NCMDS	10		/* number of commands */

				/* dev_input() returns flags from above */
#define DFL(dc)		(1<<(dc))

#define	CTRL(c)		((c)-'@')
				/* commands entered in display window */
#define DV_INIT		{'\0','v','l','p','\r',CTRL('L'),'K','R','C','q'}
				/* commands entered on stdin */
#define	DC_INIT		{"VIEW=","where","last","pause","resume","redraw",\
				"kill","restart","clobber","quit"}


/************************************************************************
 * Driver routines (all are required):


void
dev_open(nm)		: prepare the device
char	*nm;		: appropriate title bar annotation

Sets fields of odev structure and prepares the display for i/o.
The base view type, horizontal and vertical view angles and other
default parameters in odev.v should also be assigned.


int
dev_view(nv)		: set base view parameters
VIEW	*nv;		: the new view

Updates the display for the given base view change.
Look for nv==&odev.v when making view current after dev_input()
returns DEV_NEWVIEW flag.  Return 1 on success, or 0 if the
new view would conflict with device requirements.  In the latter
case, reset parameters in nv to make it more agreeable, calling
error(COMMAND, "appropriate user warning").


void
dev_clear()		: clear device memory

Clear the device memory in preparation for fresh data.  Clearing
the screen is optional.


void
dev_value(c, d, p)	: register new point of light
COLR	c;		: pixel color (RGBE)
FVECT	d;		: ray direction vector
FVECT	p;		: world intersection point

Add the given color point to the display output queue.  If imm_mode is
non-zero, then values are being sent in rapid succession.  If p is NULL,
then the point is at infinity.


int
dev_flush()		: flush the output and prepare for select call

Updates display, taking any pending action required before select(2) call.
Returns non-zero if there is device input available.


int
dev_input()		: process pending display input

Called when odev struct file descriptor shows input is ready.
Returns flags indicating actions to take in the control process.
If the DC_VIEW or DC_RESIZE flag is returned, the odev
structure must be updated beforehand.

void
dev_auxcom(cmd, args)	: process auxiliary command
char	*cmd, *args;	: command name and argument string

Execute an auxiliary command (not one of those listed at the head of
this file).  The cmd argument points to the command name itself, and
the args argument points to a string with the rest of the input line.
If the command isn't known or there ARE no auxiliary commands, print
an appropriate COMMAND error message and return.


VIEW *
dev_auxview(n, hv)	: return nth auxiliary view
int	n;		: auxiliary view number
int	hv[2];		: returned horiz. and vert. image resolution

Return the nth auxiliary view associated with the current base view.
The hv entries are assigned the horizontal and vertical view resolution,
respectively.  Function returns NULL if there are no more auxiliary
views.  The zeroeth auxiliary view is the base view itself.


void
dev_close()		: close the display

Close the display device and free up resources in preparation for exit.
Set odev.v.type=0 and odev.hres=odev.vres=0 when done.


 ************************************************************************/


extern VIEW	*dev_auxview();
