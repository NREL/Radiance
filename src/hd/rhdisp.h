/* Copyright (c) 1997 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * Header for holodeck display drivers.
 * Include after "rholo.h".
 */

				/* display requests */
#define DR_NEWSET	1		/* new bundle set */
#define DR_BUNDLE	2		/* bundle request */
#define DR_ATTEN	3		/* request for attention */
#define DR_SHUTDOWN	4		/* shutdown request */
#define DR_ADDSET	5		/* add to current set */
#define DR_DELSET	6		/* delete from current set */

				/* server responses */
#define DS_IMMSET	1		/* immediate bundle set */
#define DS_BUNDLE	2		/* computed bundle */
#define DS_ACKNOW	3		/* acknowledge request for attention */
#define DS_SHUTDOWN	4		/* end process and close connection */
#define DS_ADDHOLO	5		/* register new holodeck */

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

			/* DR_NEWSET */
/* no body */
			/* DR_BUNDLE */
#define BUNDLE_REQ	PACKHEAD
			/* DR_ENDSET */
/* no body */
			/* DR_SHUTDOWN */
/* no body */
			/* DR_ADDSET */
/* no body */

	/* server response message bodies */

			/* DS_STARTIMM */
/* no body */
			/* DS_BUNDLE */
#define BUNDLE_RES	PACKHEAD	/* extendable */
			/* DS_ENDIMM */
/* no body */
			/* DS_SHUTDOWN */
/* no body */
			/* DS_ADDHOLO */
#define HOLO_RES	HDGRID
			/* DS_ACKNOW */
/* no body */
