/* Copyright 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * x11raster.c - routines to handle images for X windows.
 *
 *	2/18/88
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "x11raster.h"


XRASTER *
make_raster(disp, scrn, depth, data, width, height, bm_pad)
Display	*disp;
int	scrn;
int	depth;
char	*data;
int	width, height;
int	bm_pad;
{
	register XRASTER	*xr;
	XVisualInfo	ourvinfo;
						/* Pick appropriate Visual */
	if (depth == 1) {
		ourvinfo.visual = DefaultVisual(disp,scrn);
	} else if (depth == 8) {
		if (!XMatchVisualInfo(disp,scrn,8,PseudoColor,&ourvinfo))
			return(NULL);
	} else if (depth > 12) {
		if (!XMatchVisualInfo(disp,scrn,24,TrueColor,&ourvinfo))
			return(NULL);
	} else
		return(NULL);
	if ((xr = (XRASTER *)calloc(1, sizeof(XRASTER))) == NULL)
		return(NULL);
	xr->disp = disp;
	xr->screen = scrn;
	xr->visual = ourvinfo.visual;
	xr->image = XCreateImage(disp,ourvinfo.visual,depth,
			ZPixmap,0,data,width,height,bm_pad,0);
	xr->gc = XCreateGC(disp, RootWindow(disp,scrn), 0, 0);
	XSetState(disp, xr->gc, BlackPixel(disp,scrn), WhitePixel(disp,scrn),
			GXcopy, ~0L);
	return(xr);
}


int
init_rcolors(xr, rmap, gmap, bmap)		/* initialize colors */
register XRASTER	*xr;
unsigned char	rmap[256], gmap[256], bmap[256];
{
	register unsigned char	*p;
	register int	i;

	if (xr->image->depth != 8 || xr->ncolors != 0)
		return(xr->ncolors);
	xr->pmap = (short *)malloc(256*sizeof(short));
	if (xr->pmap == NULL)
		return(0);
	xr->cdefs = (XColor *)malloc(256*sizeof(XColor));
	if (xr->cdefs == NULL)
		return(0);
	for (i = 0; i < 256; i++)
		xr->pmap[i] = -1;
	for (p = (unsigned char *)xr->image->data,
			i = xr->image->width*xr->image->height;
			i--; p++)
		if (xr->pmap[*p] == -1) {
			xr->cdefs[xr->ncolors].red = rmap[*p] << 8;
			xr->cdefs[xr->ncolors].green = gmap[*p] << 8;
			xr->cdefs[xr->ncolors].blue = bmap[*p] << 8;
			xr->cdefs[xr->ncolors].pixel = *p;
			xr->cdefs[xr->ncolors].flags = DoRed|DoGreen|DoBlue;
			xr->pmap[*p] = xr->ncolors++;
		}
	xr->cdefs = (XColor *)realloc((char *)xr->cdefs,
			xr->ncolors*sizeof(XColor));
	if (xr->cdefs == NULL)
		return(0);
	return(xr->ncolors);
}


unsigned long *
map_rcolors(xr, w)				/* get and assign pixels */
register XRASTER	*xr;
Window	w;
{
	register int	i;
	register unsigned char	*p;
	int	j;

	if (xr->ncolors == 0 || xr->image->depth != 8)
		return(NULL);
	if (xr->pixels != NULL)
		return(xr->pixels);
	xr->pixels = (unsigned long *)malloc(xr->ncolors*sizeof(unsigned long));
	if (xr->pixels == NULL)
		return(NULL);
	if (xr->visual == DefaultVisual(xr->disp, xr->screen)) {
		xr->cmap = DefaultColormap(xr->disp, xr->screen);
		goto gotmap;
	}
getmap:
	xr->cmap = XCreateColormap(xr->disp, w, xr->visual, AllocNone);
gotmap:
	if (XAllocColorCells(xr->disp, xr->cmap, 0,
			&j, 0, xr->pixels, xr->ncolors) == 0) {
		if (xr->cmap == DefaultColormap(xr->disp, xr->screen))
			goto getmap;
		free((char *)xr->pixels);
		xr->pixels = NULL;
		return(NULL);
	}
	for (i = 0; i < xr->ncolors; i++)
		if (xr->pmap[xr->pixels[i]] == -1)
			break;
	if (i < xr->ncolors) {			/* different pixels */
		for (p = (unsigned char *)xr->image->data,
				i = xr->image->width*xr->image->height;
				i--; p++)
			*p = xr->pixels[xr->pmap[*p]];
		for (i = 0; i < 256; i++)
			xr->pmap[i] = -1;
		for (i = 0; i < xr->ncolors; i++) {
			xr->cdefs[i].pixel = xr->pixels[i];
			xr->pmap[xr->pixels[i]] = i;
		}
		free_rpixmap(xr);		/* Pixmap invalid */
	}
	XStoreColors(xr->disp, xr->cmap, xr->cdefs, xr->ncolors);
	XSetWindowColormap(xr->disp, w, xr->cmap);
	return(xr->pixels);
}


Pixmap
make_rpixmap(xr)			/* make pixmap for raster */
register XRASTER	*xr;
{
	Pixmap	pm;

	if (xr->pm != 0)
		return(xr->pm);
	pm = XCreatePixmap(xr->disp, RootWindow(xr->disp, xr->screen),
			xr->image->width, xr->image->height, xr->image->depth);
	if (pm == 0)
		return(0);
	put_raster(pm, 0, 0, xr);
	return(xr->pm = pm);
}


unmap_rcolors(xr)			/* free colors */
register XRASTER	*xr;
{
	if (xr->pixels == NULL)
		return;
	XFreeColors(xr->disp, xr->cmap, xr->pixels, xr->ncolors, 0);
	if (xr->cmap != DefaultColormap(xr->disp, xr->screen))
		XFreeColormap(xr->disp, xr->cmap);
	free((char *)xr->pixels);
	xr->pixels = NULL;
}


free_rpixmap(xr)			/* release Pixmap */
register XRASTER	*xr;
{
	if (xr->pm == 0)
		return;
	XFreePixmap(xr->disp, xr->pm);
	xr->pm = 0;
}


free_raster(xr)				/* free raster data */
register XRASTER	*xr;
{
	free_rpixmap(xr);
	if (xr->ncolors > 0) {
		unmap_rcolors(xr);
		free((char *)xr->pmap);
		free((char *)xr->cdefs);
	}
	XDestroyImage(xr->image);
	XFreeGC(xr->disp, xr->gc);
	free((char *)xr);
}
