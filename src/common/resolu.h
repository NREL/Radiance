/* RCSid $Id: resolu.h,v 2.15 2020/04/06 04:00:08 greg Exp $ */
/*
 * Definitions for resolution line in image file.
 *
 * Include after <stdio.h>
 *
 * True image orientation is defined by an xy coordinate system
 * whose origin is at the lower left corner of the image, with
 * x increasing to the right and y increasing in the upward direction.
 * This true orientation is independent of how the pixels are actually
 * ordered in the file, which is indicated by the resolution line.
 * This line is of the form "{+-}{XY} xyres {+-}{YX} yxres\n".
 * A typical line for a 1024x600 image might be "-Y 600 +X 1024\n",
 * indicating that the scanlines are in English text order (PIXSTANDARD).
 */
#ifndef _RAD_RESOLU_H_
#define _RAD_RESOLU_H_

#ifdef __cplusplus
extern "C" {
#endif

			/* flags for scanline ordering */
#define  XDECR			1
#define  YDECR			2
#define  YMAJOR			4

			/* standard scanline ordering */
#define  PIXSTANDARD		(YMAJOR|YDECR)
#define  PIXSTDFMT		"-Y %d +X %d\n"

			/* structure for image dimensions */
typedef struct {
	int	rt;		/* orientation (from flags above) */
	int	xr, yr;		/* x and y resolution */
} RESOLU;

			/* macros to get scanline length and number */
#define  scanlen(rs)		((rs)->rt & YMAJOR ? (rs)->xr : (rs)->yr)
#define  numscans(rs)		((rs)->rt & YMAJOR ? (rs)->yr : (rs)->xr)

			/* resolution string buffer and its size */
#define  RESOLU_BUFLEN		32
extern char  resolu_buf[RESOLU_BUFLEN];

			/* macros for reading/writing resolution struct */
#define  fputsresolu(rs,fp)	fputs(resolu2str(resolu_buf,rs),fp)
#define  fgetsresolu(rs,fp)	str2resolu(rs, \
					fgets(resolu_buf,RESOLU_BUFLEN,fp))

			/* reading/writing of standard ordering */
#define  fprtresolu(sl,ns,fp)	fprintf(fp,PIXSTDFMT,ns,sl)
#define  fscnresolu(sl,ns,fp)	(fgetresolu(sl,ns,fp)==PIXSTANDARD)

			/* defined in resolu.c */
extern void	fputresolu(int ord, int sl, int ns, FILE *fp);
extern int	fgetresolu(int *sl, int *ns, FILE *fp);
extern char *	resolu2str(char *buf, RESOLU *rp);
extern int	str2resolu(RESOLU *rp, char *buf);

#ifdef __cplusplus
}
#endif
#endif /* _RAD_RESOLU_H_ */

