#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Return Radiance library search path
 *
 *  External symbols declared in rtio.h
 */

#include "copyright.h"

#include <stdio.h>

#include "rtio.h"
#include "paths.h"

char *
getrlibpath()
{
	static char	*libpath = NULL;

	if (libpath == NULL)
		if ((libpath = getenv(ULIBVAR)) == NULL)
			libpath = DEFPATH;

	return(libpath);
}
