/* Copyright (c) 1997 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for OpenGL display process.
 */

#ifndef	MEM_PTR
#define MEM_PTR		char *
#endif
#ifndef int4
#define	int4		int		/* assume 32-bit integers */
#endif

				/* request types */
#define GD_R_ViewSync	0		/* synchronize current view */
#define GD_R_SetView	1		/* set new view parameters */
#define GD_R_AddBg	2		/* add background rectangle */
#define GD_R_DelBg	3		/* delete background rectangle */
#define GD_R_Error	4		/* error to report */
#define GD_R_Init	5		/* initialize connection */
#define GD_NREQ		6		/* number of requests */

				/* error codes */
#define GD_OK		0		/* normal return value */
#define GD_E_UNRECOG	1		/* unrecognized request */
#define GD_E_ARGMISS	2		/* missing argument(s) */
#define GD_E_NOMEMOR	3		/* out of memory */
#define GD_E_CONNECT	4		/* can't establish connection */
#define GD_NERRS	5		/* number of errors */

extern char	*gdErrMsg[GD_NERRS];	/* our error message list */

				/* request structure */
#define GD_ARG0		4		/* minimum argument length */
typedef struct {
	short		type;		/* request type */
	unsigned int4	id;		/* unique identifier */
	unsigned int4	alen;		/* argument buffer length */
	unsigned char	args[GD_ARG0];	/* followed by the actual arguments */
} GDrequest;			/* a GL display request */

typedef struct {
	int	cno;		/* connection number */
	int	xres, yres;	/* display window size (renderable area) */
	int	fdout;		/* send descriptor */
	FILE	*fpin;		/* receive stream */
} GDconnect;		/* display server/client connection */

				/* argument lengths */
#define GD_L_REAL	5		/* reals are 5 bytes */
#define gdStrLen(s)	(strlen(s)+1)	/* strings are nul-terminated */

#define gdFree(p)	free((MEM_PTR)(p))

extern GDrequest	*gdRecvRequest();
extern GDconnect	*gdOpen();
extern int4	gdGetInt();
extern double	gdGetReal();
