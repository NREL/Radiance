/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Microsoft Mouse handling routines
 */

#include <stdio.h>
#include <dos.h>
#include <i86.h>
#include <graph.h>
#include "driver.h"

#define	 M_RIGHTBUTT	0x8
#define	 M_LEFTBUTT	0x2
#define	 M_MOTION	0x1

static int ydispsize;
static int crad;
#define right_button (mouse_event & M_RIGHTBUTT)
#define left_button (mouse_event & M_LEFTBUTT)
static int mouse_event = 0;
static int mouse_xpos = -1;
static int mouse_ypos = -1;


#pragma off (check_stack)
static void _loadds far mouse_handler (int max, int mcx, int mdx)
{
#pragma aux mouse_handler parm [EAX] [ECX] [EDX]
	mouse_event = max;
	mouse_xpos = mcx;
	mouse_ypos = mdx;
}
#pragma on (check_stack)


static void
move_cursor(newx, newy)		/* move cursor to new position */
int  newx, newy;
{
	extern char  *bmalloc();
	static char  *imp = NULL;
	static int  curx = -1, cury = -1;

	if (imp == NULL &&
		(imp = bmalloc(_imagesize(0,0,2*crad+1,2*crad+1))) == NULL) {
		eputs("out of memory in move_cursor");
		quit(1);
	}
	if (curx >= 0 & cury >= 0)	/* clear old cursor */
		_putimage(curx-crad, cury-crad, imp, _GPSET);
					/* record new position */
	curx = newx; cury = newy;
	if (curx < 0 | cury < 0)
		return;		/* no cursor */
					/* save under new cursor */
	_getimage(newx-crad,newy-crad,newx+crad,newy+crad, imp);
					/* draw new cursor */
	_setcolor(1);
	_rectangle(_GFILLINTERIOR, newx-crad, newy-1, newx+crad, newy+1);
	_rectangle(_GFILLINTERIOR, newx-1, newy-crad, newx+1, newy+crad);
	_setcolor(0);
	_moveto(newx-crad+1, newy);
	_lineto(newx+crad-1, newy);
	_moveto(newx, newy-crad+1);
	_lineto(newx, newy+crad-1);
}


static int
ms_getcur(xp, yp)		/* get mouse cursor position */
int	*xp, *yp;
{
    /* show cursor */

    move_cursor(mouse_xpos, mouse_ypos);

    /* update cursor until button pressed */

    do {
	mouse_event = 0;
	while( !mouse_event )
		if (kbhit())
			switch (getch()) {
			case MB1:
				mouse_event = M_LEFTBUTT;
				break;
			case MB2:
			case MB3:
				mouse_event = M_RIGHTBUTT;
				break;
			default:
				mouse_event = M_RIGHTBUTT | M_LEFTBUTT;
				break;
			}
	if (mouse_event & M_MOTION)
		move_cursor(mouse_xpos, mouse_ypos);
    } while (!(mouse_event & (M_RIGHTBUTT|M_LEFTBUTT)));

    /* clear cursor */

    move_cursor(-1, -1);

    /* compute final position */

    if (mouse_xpos < 0 | mouse_ypos < 0)
	return(ABORT);

    *xp = mouse_xpos;
    *yp = ydispsize-1 - mouse_ypos;

    switch (mouse_event) {
    case M_LEFTBUTT:
    case M_LEFTBUTT|M_MOTION:
	return(MB1);
    case M_RIGHTBUTT:
    case M_RIGHTBUTT|M_MOTION:
	return(MB2);
    }
    return(ABORT);
}


void
ms_gcinit( dp )
struct driver  *dp;
{
    struct SREGS sregs;
    union REGS inregs, outregs;
    int far *ptr;
    int (far *function_ptr)();

    segread(&sregs);

    /* check for mouse driver */

    inregs.w.ax = 0;
    int386 (0x33, &inregs, &outregs);
    if( outregs.w.ax != -1 ) {
	dp->getcur = NULL;
	return;
    }

    /* get relevant parameters */

    ydispsize = dp->ysiz;
    crad = dp->ysiz/40;

    /* install watcher */

    inregs.w.ax = 0xC;
    inregs.w.cx = M_RIGHTBUTT | M_LEFTBUTT | M_MOTION;
    function_ptr = mouse_handler;
    inregs.x.edx = FP_OFF( function_ptr );
    sregs.es	 = FP_SEG( function_ptr );
    int386x( 0x33, &inregs, &outregs, &sregs );

    dp->getcur = ms_getcur;
}
