#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 *  System calls for meta-file routines
 */

#include "rtprocess.h" /* getpid() */
#include "rterror.h"
#include  "meta.h"


FILE *
efopen(fname, mode)		/* open a file, report errors */

char  *fname, *mode;

{
 register FILE  *fp;
 FILE  *fopen();

 if ((fp = fopen(fname, mode)) == NULL)  {
    sprintf(errmsg, "cannot open file \"%s\", mode \"%s\"", fname, mode);
    error(USER, errmsg);
    }

 return(fp);
 }



FILE *
mfopen(fname, mode)		/* open a program metafile */

char  *fname;
char  *mode;

{
    char  *mdir, stemp[MAXFNAME];
    char  *getenv();

    if ((mdir = getenv("MDIR")) == NULL)
	mdir = MDIR;
    sprintf(stemp, "%s%s", mdir, fname);

    return(efopen(stemp, mode));
}


