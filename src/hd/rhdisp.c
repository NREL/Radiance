/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Holodeck display process.
 */

#include "rholo.h"
#include "rhdisp.h"
#include "rhdriver.h"
#include "selcall.h"

HOLO	*hdlist[HDMAX+1];	/* global holodeck list */

int	imm_mode = 0;		/* bundles are being delivered immediately */

VIEW	dvw;			/* our current display view */

char	*progname;		/* global argv[0] */

#define SERV_READY	01
#define DEV_READY	02


main(argc, argv)
int	argc;
char	*argv[];
{
	int	rdy, inp, res = 0;

	progname = argv[0];
	if (argc != 2)
		error(USER, "bad command line arguments");
					/* open our device */
	dev_open(argv[1]);
					/* enter main loop */
	do {
		rdy = disp_wait();
		if (rdy & DEV_READY) {
			inp = dev_input();	/* take residual action here */
			if (inp & DEV_SHUTDOWN)
				serv_request(DR_SHUTDOWN, 0, NULL);
			else if (inp & DEV_NEWVIEW)
				new_view(&odev.v);
		}
		if (rdy & SERV_READY)
			res = serv_result();	/* processes result, also */
	} while (res != DS_SHUTDOWN);
					/* all done */
	quit(0);
}


int
disp_wait()			/* wait for more input */
{
	fd_set	readset, errset;
	int	flgs;
	int	n;
	register int	i;
				/* see if we can avoid select call */
	if (imm_mode || stdin->_cnt > 0)
		return(SERV_READY);
	if (dev_flush())
		return(DEV_READY);
				/* make the call */
	FD_ZERO(&readset); FD_ZERO(&errset);
	FD_SET(0, &readset);
	FD_SET(0, &errset);
	FD_SET(odev.ifd, &readset);
	FD_SET(odev.ifd, &errset);
	n = odev.ifd + 1;
	n = select(n, &readset, NULL, &errset, NULL);
	if (n < 0) {
		if (errno == EINTR)
			return(0);
		error(SYSTEM, "select call failure in disp_wait");
	}
	flgs = 0;		/* flag what's ready */
	if (FD_ISSET(0, &readset) || FD_ISSET(0, &errset))
		flgs |= SERV_READY;
	if (FD_ISSET(odev.ifd, &readset) || FD_ISSET(odev.ifd, &errset))
		flgs |= DEV_READY;
	return(flgs);
}


add_holo(hdg)			/* register a new holodeck section */
HDGRID	*hdg;
{
	VIEW	nv;
	register int	hd;

	for (hd = 0; hd < HDMAX && hdlist[hd] != NULL; hd++)
		;
	if (hd >= HDMAX)
		error(INTERNAL, "too many holodeck sections in add_holo");
	hdlist[hd] = (HOLO *)malloc(sizeof(HOLO));
	if (hdlist[hd] == NULL)
		error(SYSTEM, "out of memory in add_holo");
	bcopy((char *)hdg, (char *)hdlist[hd], sizeof(HDGRID));
	hdcompgrid(hdlist[hd]);
	if (hd)
		return;
					/* set initial viewpoint */
	copystruct(&nv, &odev.v);
	VSUM(nv.vp, hdlist[0]->orig, hdlist[0]->xv[0], 0.5);
	VSUM(nv.vp, nv.vp, hdlist[0]->xv[1], 0.5);
	VSUM(nv.vp, nv.vp, hdlist[0]->xv[2], 0.5);
	fcross(nv.vdir, hdlist[0]->xv[1], hdlist[0]->xv[2]);
	VCOPY(nv.vup, hdlist[0]->xv[2]);
	new_view(&nv);
}


disp_bundle(p)			/* display a ray bundle */
register PACKHEAD	*p;
{
	GCOORD	gc[2];
	FVECT	ro, rd, wp;
	double	d;
	register int	i;
					/* get beam coordinates */
	if (p->hd < 0 | p->hd >= HDMAX || hdlist[p->hd] == NULL)
		error(INTERNAL, "bad holodeck number in disp_bundle");
	if (!hdbcoord(gc, hdlist[p->hd], p->bi))
		error(INTERNAL, "bad beam index in disp_bundle");
					/* display each ray */
	for (i = p->nr; i--; ) {
		hdray(ro, rd, hdlist[p->hd], gc, packra(p)[i].r);
		d = hddepth(hdlist[p->hd], packra(p)[i].d);
		VSUM(wp, ro, rd, d);		/* might be behind viewpoint */
		dev_value(packra(p)[i].v, wp);
	}
}


new_view(v)			/* change view parameters */
VIEW	*v;
{
	char	*err;

	if ((err = setview(v)) != NULL)
		error(INTERNAL, err);
	dev_view(v);			/* update display driver */
	beam_view(&dvw, v);		/* update beam list */
	copystruct(&dvw, v);		/* record new view */
}


int
serv_result()			/* get next server result and process it */
{
	static char	*buf = NULL;
	static int	bufsiz = 0;
	MSGHEAD	msg;
	int	n;
					/* read message header */
	if (fread((char *)&msg, sizeof(MSGHEAD), 1, stdin) != 1)
		goto readerr;
	if (msg.nbytes > 0) {		/* get the message body */
		if (msg.nbytes > bufsiz) {
			if (buf == NULL)
				buf = (char *)malloc(bufsiz=msg.nbytes);
			else
				buf = (char *)realloc(buf, bufsiz=msg.nbytes);
			if (buf == NULL)
				error(SYSTEM, "out of memory in serv_result");
		}
		if (fread(buf, 1, msg.nbytes, stdin) != msg.nbytes)
			goto readerr;
	}
	switch (msg.type) {		/* process results */
	case DS_BUNDLE:
		if (msg.nbytes < sizeof(PACKHEAD) ||
				msg.nbytes != packsiz(((PACKHEAD *)buf)->nr))
			error(INTERNAL, "bad display packet from server");
		disp_bundle((PACKHEAD *)buf);
		break;
	case DS_ADDHOLO:
		if (msg.nbytes != sizeof(HDGRID))
			error(INTERNAL, "bad holodeck record from server");
		add_holo((HDGRID *)buf);
		break;
	case DS_STARTIMM:
	case DS_ENDIMM:
		imm_mode = msg.type==DS_STARTIMM;
		/* fall through */
	case DS_ACKNOW:
	case DS_SHUTDOWN:
		if (msg.nbytes) {
			sprintf(errmsg,
				"unexpected body with server message %d",
					msg.type);
			error(INTERNAL, errmsg);
		}
		break;
	default:
		error(INTERNAL, "unrecognized result from server process");
	}
	return(msg.type);		/* return message type */
readerr:
	if (feof(stdin))
		error(SYSTEM, "server process died");
	error(SYSTEM, "error reading from server process");
}


serv_request(type, nbytes, p)	/* send a request to the server process */
int	type, nbytes;
char	*p;
{
	MSGHEAD	msg;
	int	m;
				/* get server's attention for big request */
	if (nbytes >= BIGREQSIZ-sizeof(MSGHEAD)) {
		serv_request(DR_ATTEN, 0, NULL);
		while ((m = serv_result()) != DS_ACKNOW)
			if (m == DS_SHUTDOWN)	/* the bugger quit on us */
				quit(0);
	}
	msg.type = type;	/* write and flush the message */
	msg.nbytes = nbytes;
	fwrite((char *)&msg, sizeof(MSGHEAD), 1, stdout);
	if (nbytes > 0)
		fwrite(p, 1, nbytes, stdout);
	if (fflush(stdout) < 0)
		error(SYSTEM, "write error in serv_request");
}


quit(code)			/* clean up and exit */
int	code;
{
	if (odev.v.type)
		dev_close();
	exit(code);
}
