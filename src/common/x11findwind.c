/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * find a window by its name under X
 *
 *	4/22/91		Greg Ward
 */

#include <stdio.h>
#include <X11/Xlib.h>


Window
xfindwind(dpy, win, name, depth)
Display	*dpy;
Window	win;
char	*name;
int	depth;
{
	char	*nr;
	Window	rr, pr, *cl;
	unsigned int	nc;

	if (XFetchName(dpy, win, &nr)) {
		register int	succ = !strcmp(nr, name);
		free(nr);
		if (succ) return(win);
	}
	if (depth == 0)		/* negative depths search all */
		return(None);
	if (!XQueryTree(dpy, win, &rr, &pr, &cl, &nc) || nc == 0)
		return(None);
	while (nc--)
		if ((rr = xfindwind(dpy, cl[nc], name, depth-1)) != None)
			break;
	XFree((char *)cl);
	return(rr);
}
