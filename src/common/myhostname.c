/* Copyright (c) 1996 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Query system for host name
 */

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
