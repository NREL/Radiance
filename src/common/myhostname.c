#ifndef lint
static const char	RCSid[] = "$Id$";
#endif
/*
 * Query system for host name
 */

#include "copyright.h"

#ifndef BSD

#include  <sys/utsname.h>

char *
myhostname()
{
	static struct utsname	nambuf;

	if (!nambuf.nodename[0])
		uname(&nambuf);
	return(nambuf.nodename);
}

#else

char *
myhostname()
{
	static char	hostname[65];

	if (!hostname[0])
		gethostname(hostname, sizeof(hostname));
	return(hostname);
}

#endif
