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
	Window	wr;
	unsigned int	nc;
	register int	i;

	if (depth == 0)		/* negative depths search all */
		return(None);
	if (!XQueryTree(dpy, win, &rr, &pr, &cl, &nc) || nc == 0)
		return(None);
	wr = None;		/* breadth first search */
	for (i = 0; wr == None && i < nc; i++)
		if (XFetchName(dpy, cl[i], &nr)) {
			if (!strcmp(nr, name))
				wr = cl[i];
			free(nr);
		}
	for (i = 0; wr == None && i < nc; i++)
		wr = xfindwind(dpy, cl[i], name, depth-1);
	XFree((char *)cl);
	return(wr);
}
