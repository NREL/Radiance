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
	} else if (depth == 24) {
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
			depth==1 ? XYBitmap : ZPixmap,
			0,data,width,height,bm_pad,0);
	xr->image->bitmap_bit_order = MSBFirst;
	xr->gc = XCreateGC(disp, RootWindow(disp,scrn), 0, 0);
	XSetState(disp, xr->gc, BlackPixel(disp,scrn), WhitePixel(disp,scrn),
			GXcopy, AllPlanes);
	return(xr);
}


int
init_rcolors(xr, rmap, gmap, bmap)		/* initialize colors */
register XRASTER	*xr;
int	rmap[256], gmap[256], bmap[256];
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

Colormap
newcmap(disp, scrn, w, vis)		/* get colormap and fix b & w */
Display	*disp;
int	scrn;
Window	w;
Visual	*vis;
{
	XColor	thiscolor;
	unsigned long	*pixels;
	Colormap	cmap;
	int	n;
	register int	i, j;

	cmap = XCreateColormap(disp, w, vis, AllocNone);
	if (cmap == 0)
		return(0);
	pixels=(unsigned long *)malloc(vis->map_entries*sizeof(unsigned long));
	if (pixels == NULL)
		return(0);
	for (n = vis->map_entries; n > 0; n--)
		if (XAllocColorCells(disp, cmap, 0, NULL, 0, pixels, n) != 0)
			break;
	if (n == 0)
		return(0);
					/* reset black and white */
	for (i = 0; i < n; i++) {
		if (pixels[i] != BlackPixel(disp,scrn)
				&& pixels[i] != WhitePixel(disp,scrn))
			continue;
		thiscolor.pixel = pixels[i];
		thiscolor.flags = DoRed|DoGreen|DoBlue;
		XQueryColor(disp, DefaultColormap(disp,scrn), &thiscolor);
		XStoreColor(disp, cmap, &thiscolor);
		for (j = i; j+1 < n; j++)
			pixels[j] = pixels[j+1];
		n--;
		i--;
	}
	XFreeColors(disp, cmap, pixels, n, 0);
	free((char *)pixels);
	return(cmap);
}


unsigned long *
map_rcolors(xr, w)				/* get and assign pixels */
register XRASTER	*xr;
Window	w;
{
	register int	i;
	register unsigned char	*p;

	if (xr->ncolors == 0 || xr->image->depth != 8)
		return(NULL);
	if (xr->pixels != NULL)
		return(xr->pixels);
	xr->pixels = (unsigned long *)malloc(xr->ncolors*sizeof(unsigned long));
	if (xr->pixels == NULL)
		return(NULL);
	if (xr->visual == DefaultVisual(xr->disp, xr->screen))
		xr->cmap = DefaultColormap(xr->disp, xr->screen);
	else
		xr->cmap = newcmap(xr->disp, xr->screen, w, xr->visual);
	while (XAllocColorCells(xr->disp, xr->cmap, 0,
			NULL, 0, xr->pixels, xr->ncolors) == 0)
		if (xr->cmap == DefaultColormap(xr->disp, xr->screen))
			xr->cmap = newcmap(xr->disp, xr->screen, w, xr->visual);
		else {
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
			xr->image->width, xr->image->height,
			DisplayPlanes(xr->disp, xr->screen));
	if (pm == 0)
		return(0);
	put_raster(pm, 0, 0, xr);
	return(xr->pm = pm);
}


patch_raster(d, xsrc, ysrc, xdst, ydst, width, height, xr)	/* redraw */
Drawable	d;
int	xsrc, ysrc, xdst, ydst;
int	width, height;
register XRASTER	*xr;
{
	if (xsrc >= xr->image->width || ysrc >= xr->image->height)
		return;
	if (xsrc < 0) {
		xdst -= xsrc; width += xsrc;
		xsrc = 0;
	}
	if (ysrc < 0) {
		ydst -= ysrc; height += ysrc;
		ysrc = 0;
	}
	if (width <= 0 || height <= 0)
		return;
	if (xsrc + width > xr->image->width)
		width = xr->image->width - xsrc;
	if (ysrc + height > xr->image->height)
		height = xr->image->height - ysrc;

	if (xr->pm == 0)
		XPutImage(xr->disp, d, xr->gc, xr->image, xsrc, ysrc,
				xdst, ydst, width, height);
	else
		XCopyArea(xr->disp, xr->pm, d, xr->gc, xsrc, ysrc,
				width, height, xdst, ydst);
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
