/* RCSid $Id$ */
/*
 * Header for routines using vfork() system call.
 */

#if !defined(BSD) || defined(sparc)

#define	vfork	fork

#endif

extern int	vfork();
