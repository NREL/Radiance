/* Copyright (c) 1995 Regents of the University of California */

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


main(argc, argv)	/* first argument is understood entities, comma-sep. */
int	argc;
char	*argv[];
{
	char	*cp1, *cp2;
	int	i, en;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s entity,list [file ..]\n", argv[0]);
		exit(1);
	}
	for (cp1 = cp2 = argv[1]; *cp1; cp1 = cp2) {
		while (*cp2) {
			if (*cp2 == ',') {
				*cp2++ = '\0';
				break;
			}
			cp2++;
		}
		en = mg_entity(cp1);
		if (en < 0) {
			fprintf(stderr, "%s: %s: no such entity\n",
					argv[0], cp1);
			exit(1);
		}
		mg_ehand[en] = put_entity;
	}
	mg_init();
	if (argc < 3)
		exit(mg_load((char *)NULL) != MG_OK);
	for (i = 2; i < argc; i++)
		if (mg_load(argv[i]) != MG_OK)
			exit(1);
	exit(0);
}
