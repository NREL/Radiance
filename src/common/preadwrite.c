#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Substitute routines for pread(2) and pwrite(2)
 */

#include "platform.h"


int
pread(fd, buf, siz, offs)	/* read buffer from an open file */
int	fd;
char	*buf;
unsigned int	siz;
long	offs;
{
	if (lseek(fd, (off_t)offs, SEEK_SET) != offs)
		return(-1);
	return(read(fd, buf, siz));
				/* technically, we should reset pointer here */
}


int
pwrite(fd, buf, siz, offs)	/* write buffer to an open file */
int	fd;
char	*buf;
unsigned int	siz;
long	offs;
{
	if (lseek(fd, (off_t)offs, SEEK_SET) != offs)
		return(-1);
	return(write(fd, buf, siz));
				/* technically, we should reset pointer here */
}
