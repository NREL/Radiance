/* Copyright (c) 1992 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header for routines using vfork() system call.
 */

#ifdef BSD

#ifdef sparc
#include "/usr/include/vfork.h"
#else
extern int	vfork();
#endif

#else

extern int	fork();

#define	vfork	fork

#endif
