/* Copyright (c) 1997 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * Header for holodeck display drivers.
 * Include after "rholo.h".
 */

				/* display requests */
#define DR_BUNDLE	1		/* bundle request */
#define DR_ATTEN	2		/* request for attention */
#define DR_SHUTDOWN	3		/* shutdown request */
#define DR_NEWSET	4		/* new bundle set */
#define DR_ADDSET	5		/* add to current set */
#define DR_DELSET	6		/* delete from current set */

				/* server responses */
#define DS_BUNDLE	1		/* computed bundle */
#define DS_ACKNOW	2		/* acknowledge request for attention */
#define DS_SHUTDOWN	3		/* end process and close connection */
#define DS_ADDHOLO	4		/* register new holodeck */
#define DS_STARTIMM	5		/* begin immediate bundle set */
#define DS_ENDIMM	6		/* end immediate bundle set */

/*
 * Normally, the server channel has priority, with the display process
 * checking it frequently for new data.  However, when the display process
 * makes a request for attention (DR_ATTEN), the server will finish its
 * current operations and flush its buffers, sending an acknowledge
 * message (DS_ACKNOW) when it's done.  This then allows the
 * display process to send a long request to the holodeck server.
 * Priority returns to normal after the following request.
 */

typedef struct {
	int2	type;		/* message type */
	int4	nbytes;		/* number of additional bytes */
} MSGHEAD;		/* message head */

	/* display request message bodies */

			/* DR_BUNDLE */
#define BUNDLE_REQ	PACKHEAD
			/* DR_ATTEN */
/* no body */
			/* DR_SHUTDOWN */
/* no body */
			/* DR_NEWSET */
/* body is nbytes/sizeof(BUNDLE_REQ) BUNDLE_REQ bodies */
			/* DR_ADDSET */
/* body is nbytes/sizeof(PACKHEAD) BUNDLE_REQ bodies */
			/* DR_DELSET */
/* body is nbytes/sizeof(PACKHEAD) BUNDLE_REQ bodies */

	/* server response message bodies */

			/* DS_BUNDLE */
#define BUNDLE_RES	PACKHEAD	/* followed by nr RAYVAL structs */
			/* DS_ACKNOW */
/* no body */
			/* DS_SHUTDOWN */
/* no body */
			/* DS_ADDHOLO */
#define HOLO_RES	HDGRID
			/* DS_STARTIMM */
/* no body */
			/* DS_ENDIMM */
/* no body */
