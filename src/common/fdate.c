/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Return file date (UNIX seconds as returned by time(2) call)
 */

#include  <sys/types.h>
#include  <sys/stat.h>


time_t
fdate(fname)				/* get file date */
char  *fname;
{
	struct stat  sbuf;

	if (stat(fname, &sbuf) == -1)
		return(0);

	return(sbuf.st_mtime);
}


int
setfdate(fname, ftim)			/* set file date */
char  *fname;
long  ftim;
{
	time_t  ftm[2];

	ftm[0] = ftm[1] = ftim;

	return(utime(fname, ftm));
}
