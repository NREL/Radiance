/* Copyright (c) 1992 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header for routines using vfork() system call.
 */


#if !defined(BSD) || defined(sparc)

#define	vfork	fork

#endif

extern int	vfork();
