/* Copyright (c) 1997 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * I/O routines for GL display process.
 */

#include "standard.h"
#include <sys/uio.h>
#include "gldisp.h"
#include "gderrs.h"


gdSendRequest(d, r)	/* send a request across a connection */
GDconnect	*d;
GDrequest	*r;
{
	unsigned char	rbuf[10];
	struct iovec	iov[2];
					/* encode header */
	gdPutInt(rbuf, (int4)r->type, 2);
	gdPutInt(rbuf+2, r->id, 4);
	gdPutInt(rbuf+6, r->alen, 4);
	if (r->alen == 0)		/* write header, null body */
		write(d->fdout, rbuf, 10);
	else {				/* write header and body */
		iov[0].iov_base = (MEM_PTR)rbuf;
		iov[0].iov_len = 10;
		iov[1].iov_base = (MEM_PTR)r->args;
		iov[1].iov_len = r->alen;
		writev(d->fdout, iov, 2);
	}
}


GDrequest *
gdRecvRequest(d)	/* receive a request across a connection */
register GDconnect	*d;
{
	register GDrequest	*r;
	int	type;
	int4	id, alen;
	unsigned char	rbuf[10];
					/* read header */
	fread(rbuf, 1, 10, d->fpin);
	if (feof(d->fpin))
		return(NULL);
	type = gdGetInt(rbuf, 2);
	id = gdGetInt(rbuf+2, 4);
	if (type < 0 | type > GD_NREQ) {
		gdSendError(d, GD_E_UNRECOG, id);
		return(NULL);
	}
	alen = gdGetInt(rbuf+6, 4);
	r = (GDrequest *)malloc(sizeof(GDrequest)-GD_ARG0+alen);
	if (r == NULL) {
		gdSendError(d, GD_E_NOMEMOR, id);
		return(NULL);
	}
	r->type = type;
	r->id = id;
	if ((r->alen = alen) > 0) {	/* read arguments */
		fread(r->args, 1, alen, d->fpin);
		if (feof(d->fpin)) {
			gdSendError(d, GD_E_ARGMISS, id);
			gdFree(r);
			return(NULL);
		}
	}
	if (type == GD_R_Error) {	/* our error */
		gdReportError(d, (int)gdGetInt(r->args, 2), id);
		gdFree(r);
		return(NULL);
	}
	return(r);			/* success! */
}


gdSendError(d, ec, id)	/* send fatal error associated with a request */
GDconnect	*d;
int	ec;
int4	id;
{
	GDrequest	rq;
				/* build error request */
	rq.type = GD_R_Error;
	rq.id = id;
	gdPutInt(rq.args, (int4)ec, rq.alen=2);
	gdSendRequest(d, &rq);	/* send it */
	gdClose(d);		/* all errors are fatal */
}


gdReportError(d, ec, id)	/* report fatal error to stderr */
GDconnect	*d;
int	ec;
int4	id;
{
	fprintf(stderr, "RequestId %d: %s\nConnection %d closed.\n",
			id, gdErrMsg[ec], d->cno);
	gdClose(d);		/* all errors are fatal */
}


GDconnect *
gdOpen(inp, outp, x, y)	/* establish a client/server connection */
char	*inp, *outp;		/* named pipes, currently */
int	x, y;			/* (0,0) for client */
{
	static int	nconnections = 0;
	register GDrequest	*rp;
	GDrequest	rq;
	register GDconnect	*dp;

	dp = (GDconnect *)malloc(sizeof(GDconnect));
	if (dp == NULL) {
		gdReportError(dp, GD_E_NOMEMOR, 0);
		return(NULL);
	}
	dp->cno = ++nconnections;
	if (!x)				/* client opens to write first */
		dp->fdout = open(outp, O_WRONLY);
	dp->fpin = fopen(inp, "r");
	if (dp->fpin == NULL)
		goto connecterr;
	if (x)				/* server opens to write second */
		dp->fdout = open(outp, O_WRONLY);
	if (dp->fdout < 0)
		goto connecterr;
	if (x) {			/* server sends resolution */
		dp->xres = x; dp->yres = y;
		rq.type = GD_R_Init;
		rq.id = dp->cno;
		rq.alen = 4;
		gdPutInt(rq.args, (int4)x, 2);
		gdPutInt(rq.args+2, (int4)y, 2);
		gdSendRequest(dp, &rq);
	} else {			/* client receives resolution */
		rp = gdRecvRequest(dp);
		if (rp == NULL)
			return(NULL);
		if (rp->type != GD_R_Init)
			goto connecterr;
		dp->cno = rp->id;
		dp->xres = gdGetInt(rp->args, 2);
		dp->yres = gdGetInt(rp->args+2, 2);
		gdFree(rp);
	}
	return(dp);			/* success! */
connecterr:
	gdReportError(dp, GD_E_CONNECT, 0);
	return(NULL);
}


gdClose(d)		/* close a display server/client connection */
register GDconnect	*d;
{
	if (d == NULL) return;
	close(d->fdout);
	fclose(d->fpin);
	gdFree(d);		/* free associated memory */
}


int4
gdGetInt(ab, n)		/* get an n-byte integer from buffer ab */
register unsigned char	*ab;
register int	n;			/* maximum length 4 */
{
	register int4	res;

	res = *ab&0x80 ? -1 : 0;	/* sign extend */
	while (n--)
		res = res<<8 | *ab++;
	return(res);
}


gdPutInt(ab, i, n)	/* put an n-byte integer into buffer ab */
register unsigned char	*ab;
register int4	i;
register int	n;
{
	while (n--)
		*ab++ = i>>(n<<3) & 0xff;
}


double
gdGetReal(ab)		/* get a 5-byte real value from buffer ab */
char	*ab;
{
	register int4	i;
	register double	d;

	i = gdGetInt(ab, 4);
	d = (i + (i>0 ? .5 : -.5)) * (1./0x7fffffff);
	return(ldexp(d, (int)gdGetInt(ab+4,1)));
}


gdPutReal(ab, d)	/* put a 5-byte real value to buffer ab */
unsigned char	*ab;
double	d;
{
	int	e;

	gdPutInt(ab, (int4)(frexp(d,&e)*0x7fffffff), 4);
	gdPutInt(ab+4, (int4)e, 1);
}
