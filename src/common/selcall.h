/* Copyright (c) 1997 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * header file for select call compatibility
 */

#include <sys/types.h>
#include <sys/time.h>
#ifdef INCL_SEL_H
#include <sys/select.h>
#endif

#ifndef FD_SETSIZE
#include <sys/param.h>
#define FD_SETSIZE	NOFILE		/* maximum # select file descriptors */
#endif
#ifndef FD_SET
#ifndef NFDBITS
#define NFDBITS		(8*sizeof(int))	/* number of bits per fd_mask */
#endif
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#ifndef BSD
#define	bzero(d,n)	memset(d,0,n)
#endif
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif
