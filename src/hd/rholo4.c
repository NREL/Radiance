/* Copyright (c) 1997 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Holodeck display process communication
 */

#include "rholo.h"
#include "rhdisp.h"
#include <sys/uio.h>

#ifndef HDSUF
#define HDSUF	".hdisp"
#endif

static int	inp_flags;
static int	dpd[3];
static FILE	*dpout;


disp_open(dname)		/* open the named display driver */
char	*dname;
{
	char	dpath[128], *com[3];
	int	i;

#ifdef DEVPATH
	sprintf(dpath, "%s/%s%s", DEVPATH, dname, HDSUF);
#else
	sprintf(dpath, "dev/%s%s", dname, HDSUF);
#endif
	com[0] = dpath; com[1] = froot; com[2] = NULL;
	i = open_process(dpd, com);
	if (i <= 0)
		error(USER, "cannot start display process");
	if ((dpout = fdopen(dup(dpd[1]), "w")) == NULL)
		error(SYSTEM, "cannot associate FILE with display pipe");
	inp_flags = 0;
				/* write out hologram grids */
	for (i = 0; hdlist[i] != NULL; i++)
		disp_result(DS_ADDHOLO, sizeof(HDGRID), (char *)hdlist[i]);
	disp_flush();
}


disp_packet(p)			/* display a packet */
register PACKHEAD	*p;
{
	disp_result(DS_BUNDLE, packsiz(p->nr), (char *)p);
}


disp_check(block)		/* check display process */
int	block;
{
	MSGHEAD	msg;
	int	n;
	char	*buf = NULL;

	if (dpout == NULL)
		return(-1);
					/* flush display output */
	disp_flush();
					/* check read blocking */
	if (block != (inp_flags == 0)) {
		inp_flags = block ? 0 : FNONBLK;
		if (fcntl(dpd[0], F_SETFL, inp_flags) < 0)
			goto fcntlerr;
	}
					/* read message header */
	n = read(dpd[0], (char *)&msg, sizeof(MSGHEAD));
	if (n != sizeof(MSGHEAD)) {
		if (n >= 0)
			error(USER, "display process died");
		if (errno != EAGAIN & errno != EINTR)
			goto readerr;
		return(2);		/* acceptable failure */
	}
	if (msg.nbytes) {		/* get the message body */
		buf = (char *)malloc(msg.nbytes);
		if (buf == NULL)
			error(SYSTEM, "out of memory in disp_check");
		if (fcntl(dpd[0], F_SETFL, inp_flags=0) < 0)
			goto fcntlerr;
		if (readbuf(dpd[0], buf, msg.nbytes) != msg.nbytes)
			goto readerr;
	}
	switch (msg.type) {		/* take appropriate action */
	case DR_BUNDLE:
		if (msg.nbytes != sizeof(PACKHEAD))
			error(INTERNAL, "bad DR_BUNDLE from display process");
		bundle_set(BS_ADD, (PACKHEAD *)buf, 1);
		break;
	case DR_NEWSET:
		if (msg.nbytes % sizeof(PACKHEAD))
			error(INTERNAL, "bad DR_NEWSET from display process");
		disp_result(DS_STARTIMM, 0, NULL);
		bundle_set(BS_NEW, (PACKHEAD *)buf, msg.nbytes/sizeof(PACKHEAD));
		disp_result(DS_ENDIMM, 0, NULL);
		disp_flush();
		break;
	case DR_ADDSET:
		if (msg.nbytes % sizeof(PACKHEAD))
			error(INTERNAL, "bad DR_ADDSET from display process");
		disp_result(DS_STARTIMM, 0, NULL);
		bundle_set(BS_ADD, (PACKHEAD *)buf, msg.nbytes/sizeof(PACKHEAD));
		disp_result(DS_ENDIMM, 0, NULL);
		disp_check(0);		/* hack -- delete usu. follows add */
		break;
	case DR_ADJSET:
		if (msg.nbytes % sizeof(PACKHEAD))
			error(INTERNAL, "bad DR_ADJSET from display process");
		disp_result(DS_STARTIMM, 0, NULL);
		bundle_set(BS_ADJ, (PACKHEAD *)buf, msg.nbytes/sizeof(PACKHEAD));
		disp_result(DS_ENDIMM, 0, NULL);
		disp_flush();
		break;
	case DR_DELSET:
		if (msg.nbytes % sizeof(PACKHEAD))
			error(INTERNAL, "bad DR_DELSET from display process");
		bundle_set(BS_DEL, (PACKHEAD *)buf, msg.nbytes/sizeof(PACKHEAD));
		break;
	case DR_ATTEN:
		if (msg.nbytes)
			error(INTERNAL, "bad DR_ATTEN from display process");
					/* send acknowledgement */
		disp_result(DS_ACKNOW, 0, NULL);
		return(disp_check(1));	/* block on following request */
	case DR_SHUTDOWN:
		if (msg.nbytes)
			error(INTERNAL, "bad DR_SHUTDOWN from display process");
		return(0);		/* zero return signals shutdown */
	case DR_NOOP:
		break;
	default:
		error(INTERNAL, "unrecognized request from display process");
	}
	if (msg.nbytes)			/* clean up */
		free(buf);
	return(1);			/* normal return value */
fcntlerr:
	error(SYSTEM, "cannot change display blocking mode");
readerr:
	error(SYSTEM, "error reading from display process");
}


int
disp_close()			/* close our display process */
{
	int	rval;

	if (dpout == NULL)
		return(-1);
	disp_result(DS_SHUTDOWN, 0, NULL);
	fclose(dpout);
	dpout = NULL;
	return(close_process(dpd));
}


disp_result(type, nbytes, p)	/* queue result message to display process */
int	type, nbytes;
char	*p;
{
	MSGHEAD	msg;

	if (dpout == NULL)
		return;
	msg.type = type;
	msg.nbytes = nbytes;
	fwrite((char *)&msg, sizeof(MSGHEAD), 1, dpout);
	if (nbytes > 0)
		fwrite(p, 1, nbytes, dpout);
}


disp_flush()			/* flush output to display */
{
	if (fflush(dpout) < 0)
		error(SYSTEM, "error writing to the display process");
}
