/* Copyright (c) 1986 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  plocate.h - header for 3D vector location.
 *
 *     8/28/85
 */

#define  EPSILON	1e-6		/* acceptable location error */

#define  XPOS		03		/* x position mask */
#define  YPOS		014		/* y position mask */
#define  ZPOS		060		/* z position mask */

#define  position(i)	(3<<((i)<<1))	/* macro version */

#define  BELOW		025		/* below bits */
#define  ABOVE		052		/* above bits */
