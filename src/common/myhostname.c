#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Query system for host name
 */

#include "copyright.h"

#ifdef _WIN32
  #include <winsock.h>
#else
  #include <unistd.h>
#endif

#include "rtmisc.h"

char *
myhostname()
{
	static char	hostname[65];

	if (!hostname[0])
		gethostname(hostname, sizeof(hostname));
	return(hostname);
}

