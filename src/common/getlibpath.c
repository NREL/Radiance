/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Return Radiance library search path
 */

#include <stdio.h>

#include "paths.h"


char *
getlibpath()
{
	static char	*libpath = NULL;

	if (libpath == NULL)
		if ((libpath = getenv(ULIBVAR)) == NULL)
			libpath = DEFPATH;

	return(libpath);
}
