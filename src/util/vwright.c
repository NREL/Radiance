/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Move a viewpoint the given distance to the right
 */


#include "standard.h"

#include "view.h"

VIEW	vw = STDVIEW;

char	*progname;


main(argc, argv)
int	argc;
char	*argv[];
{
	char	linebuf[256];
	char	*err;
	int	gotview = 0;
	double	dist;
	register int	i;

	progname = argv[0];
	if (argc != 2 || !isflt(argv[1])) {
		fprintf(stderr, "Usage: %s offset\n", progname);
		exit(1);
	}
	while (fgets(linebuf, sizeof(linebuf), stdin) != NULL) {
		if (linebuf[0] == '\n')
			break;
		if (isview(linebuf) && sscanview(&vw, linebuf) > 0)
			gotview++;
	}
	if (!gotview) {
		fprintf(stderr, "%s: no view on standard input\n", progname);
		exit(1);
	}
	if ((err= setview(&vw)) != NULL) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	dist = atof(argv[1])/sqrt(vw.hn2);
	for (i = 0; i < 3; i++)
		vw.vp[i] += dist*vw.hvec[i];
	fputs(VIEWSTR, stdout);
	fprintview(&vw, stdout);
	putchar('\n');
	exit(0);
}
