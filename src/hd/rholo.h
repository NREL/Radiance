/* Copyright (c) 1997 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * Header file for rholo program
 */

#include "holo.h"
#include "vars.h"

#ifndef RPACKSIZ
#define RPACKSIZ	21		/* good packet size */
#endif

typedef struct packet {
	int2	hd;		/* holodeck section (first) */
	int4	bi;		/* beam index (second) */
	int4	nr;		/* number of rays (third) */
	int4	nc;		/* number calculated (fourth) */
	RAYVAL	ra[RPACKSIZ];	/* ray values (fourth) */
	float	*offset;	/* offset array if !vbool(OBSTRUCTIONS) */
	struct packet	*next;	/* next in packet list */
} PACKET;		/* a beam packet */

typedef struct {
	int2	hd;		/* holodeck section (first) */
	int4	bi;		/* beam index (second) */
	int4	nr;		/* number of rays (third) */
	int4	nc;		/* number calculated (fourth) */
} PACKHEAD;		/* followed by ray values */

#define packsiz(nr)	(sizeof(PACKHEAD)+(nr)*sizeof(RAYVAL))
#define packra(p)	((RAYVAL *)((p)+1))

				/* input variables */
#define RENDER		0		/* rendering options */
#define SECTION		1		/* holodeck section boundaries */
#define OCTREE		2		/* octree file name */
#define RIF		3		/* rad input file */
#define TIME		4		/* maximum rendering time */
#define DISKSPACE	5		/* how much disk space to use */
#define CACHE		6		/* amount of memory to use as cache */
#define GRID		7		/* target grid size */
#define OBSTRUCTIONS	8		/* shall we track obstructions? */
#define VDIST		9		/* virtual distance calculation */
#define OCCUPANCY	10		/* expected occupancy probability */
#define REPORT		11		/* report interval and error file */

#define NRHVARS		12		/* number of variables */

#define RHVINIT { \
	{"render",	3,	0,	NULL,	catvalues}, \
	{"section",	3,	0,	NULL,	NULL}, \
	{"OCTREE",	3,	0,	NULL,	onevalue}, \
	{"RIF",		3,	0,	NULL,	onevalue}, \
	{"TIME",	2,	0,	NULL,	fltvalue}, \
	{"DISKSPACE",	3,	0,	NULL,	fltvalue}, \
	{"CACHE",	2,	0,	NULL,	fltvalue}, \
	{"GRID",	2,	0,	NULL,	fltvalue}, \
	{"OBSTRUCTIONS",3,	0,	NULL,	boolvalue}, \
	{"VDISTANCE",	2,	0,	NULL,	boolvalue}, \
	{"OCCUPANCY",	3,	0,	NULL,	onevalue}, \
	{"REPORT",	3,	0,	NULL,	onevalue}, \
}

				/* bundle set requests */
#define BS_NEW		1		/* replace current set with new one */
#define BS_ADD		2		/* add to current set */
#define BS_ADJ		3		/* adjust current set quantities */
#define BS_DEL		4		/* delete from current set */

extern char	*progname;	/* our program name */
extern char	*hdkfile;	/* holodeck file name */
extern char	froot[];	/* root file name */

extern char	*outdev;	/* output device name */

extern int	readinp;	/* read input from stdin */

extern int	force;		/* allow overwrite of holodeck */

extern int	nowarn;		/* turn warnings off? */

extern int	ncprocs;	/* number of requested compute processes */
extern int	nprocs;		/* number of running compute processes */

extern double	expval;		/* global exposure value */

extern time_t	starttime;	/* time we got started */
extern time_t	endtime;	/* time we should end by */
extern time_t	reporttime;	/* time for next report */

extern int	rtargc;		/* rtrace command */
extern char	*rtargv[];

extern PACKET	*do_packets(), *get_packets(), *flush_queue();
