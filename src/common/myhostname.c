#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Query system for host name
 */

#include "copyright.h"

#if defined(_WIN32) || defined(_WIN64)
  #include <winsock2.h>
#else
  #include <unistd.h>
#endif

#include "rtmisc.h"

extern char *
myhostname()
{
	static char	hostname[65];

	if (!hostname[0])
		gethostname(hostname, sizeof(hostname));
	return(hostname);
}

