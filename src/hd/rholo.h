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
	RAYVAL	ra[RPACKSIZ];	/* ray values (fourth) */
	float	*offset;	/* offset array if !vbool(OBSTRUCTIONS) */
	struct packet	*next;	/* next in packet list */
} PACKET;		/* a beam packet */

typedef struct {
	int2	hd;		/* holodeck section (first) */
	int4	bi;		/* beam index (second) */
	int4	nr;		/* number of rays (third) */
} PACKHEAD;		/* followed by ray values */

#define packsiz(nr)	(sizeof(PACKHEAD)+(nr)*sizeof(RAYVAL))
#define packray(p)	(RAYVAL *)((PACKHEAD *)(p)+1)

				/* input variables */
#define RENDER		0		/* rendering options */
#define VIEW		1		/* starting view */
#define SECTION		2		/* holodeck section boundaries */
#define OCTREE		3		/* octree file name */
#define RIF		4		/* rad input file */
#define EXPOSURE	5		/* section exposure value */
#define TIME		6		/* maximum rendering time */
#define DISKSPACE	7		/* how much disk space to use */
#define CACHE		8		/* amount of memory to use as cache */
#define GRID		9		/* target grid size */
#define OBSTRUCTIONS	10		/* shall we track obstructions? */
#define OCCUPANCY	11		/* expected occupancy probability */
#define REPORT		12		/* report interval and error file */

#define NRHVARS		13		/* number of variables */

#define RHVINIT { \
	{"render",	3,	0,	NULL,	catvalues}, \
	{"view",	2,	0,	NULL,	NULL}, \
	{"section",	3,	0,	NULL,	NULL}, \
	{"OCTREE",	3,	0,	NULL,	onevalue}, \
	{"RIF",		3,	0,	NULL,	onevalue}, \
	{"EXPOSURE",	3,	0,	NULL,	fltvalue}, \
	{"TIME",	2,	0,	NULL,	fltvalue}, \
	{"DISKSPACE",	3,	0,	NULL,	fltvalue}, \
	{"CACHE",	2,	0,	NULL,	fltvalue}, \
	{"GRID",	2,	0,	NULL,	fltvalue}, \
	{"OBSTRUCTIONS",3,	0,	NULL,	boolvalue}, \
	{"OCCUPANCY",	3,	0,	NULL,	onevalue}, \
	{"REPORT",	3,	0,	NULL,	onevalue}, \
}

extern char	*progname;	/* our program name */
extern char	*hdkfile;	/* holodeck file name */
extern char	froot[];	/* root file name */

extern int	nowarn;		/* turn warnings off? */

extern double	expval;		/* global exposure value */

extern int	ncprocs;	/* number of compute processes */

extern time_t	starttime;	/* time we got started */
extern time_t	endtime;	/* time we should end by */
extern time_t	reporttime;	/* time for next report */

extern int	rtargc;		/* rtrace command */
extern char	*rtargv[];

extern PACKET	*do_packets(), *get_packets(), *flush_queue();
