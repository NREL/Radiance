/* Copyright (c) 1994 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Filter MGF stream, removing entities that won't be understood
 */

#include <stdio.h>
#include "parser.h"


int
put_entity(ac, av)		/* general output routine */
register int	ac;
register char	**av;
{
	while (ac-- > 0) {
		fputs(*av++, stdout);
		putchar(ac ? ' ' : '\n');
	}
	return(MG_OK);
}


main(argc, argv)	/* arguments are understood entities */
int	argc;
char	**argv;
{
	int	i, en;

	for (i = 1; i < argc; i++) {
		en = mg_entity(argv[i]);
		if (en < 0) {
			fprintf(stderr, "%s: %s: no such entity\n",
					argv[0], argv[i]);
			exit(1);
		}
		mg_ehand[en] = put_entity;
	}
	mg_init();
	en = mg_load((char *)NULL);
	exit(en != MG_OK);
}
