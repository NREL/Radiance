/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Move a viewpoint the given distance to the right
 */


#include "standard.h"

#include "view.h"

VIEW	leftview = STDVIEW;

VIEW	rightview;

char	*progname;


main(argc, argv)
int	argc;
char	*argv[];
{
	char	linebuf[256];
	int	gotview = 0;
	double	dist;
	FVECT	v1;
	register int	i;

	progname = argv[0];
	if (argc != 2 || !isflt(argv[1])) {
		fprintf(stderr, "Usage: %s offset\n", progname);
		exit(1);
	}
	while (fgets(linebuf, sizeof(linebuf), stdin) != NULL) {
		if (linebuf[0] == '\n')
			break;
		if (isview(linebuf) && sscanview(&leftview, linebuf) > 0)
			gotview++;
	}
	if (!gotview) {
		fprintf(stderr, "%s: no view on standard input\n", progname);
		exit(1);
	}
	fcross(v1, leftview.vdir, leftview.vup);
	if (normalize(v1) == 0.) {
		fprintf(stderr,
			"%s: view direction parallel to view up vector\n",
				progname);
		exit(1);
	}
	copystruct(&rightview, &leftview);
	dist = atof(argv[1]);
	for (i = 0; i < 3; i++)
		rightview.vp[i] += dist*v1[i];
	fputs(VIEWSTR, stdout);
	fprintview(&rightview, stdout);
	putchar('\n');
	exit(0);
}
