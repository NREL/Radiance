/* Copyright (c) 1991 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Special type flags for objects used in rendering.
 * Depends on definitions in otypes.h
 */

		/* flag for materials to ignore during irradiance comp. */
#define  T_IRR_IGN	T_SP1

#define  irr_ignore(t)	(ofun[t].flags & T_IRR_IGN)
