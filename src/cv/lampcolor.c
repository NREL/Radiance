/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Do dah program to convert lamp color from table.
 */

#include <stdio.h>

extern float	*matchlamp();


main(argc, argv)
int	argc;
char	*argv[];
{
	char	*lamptab = "lamp.tab";
	float	*lcol;
	char	buf[80];

	if (argc > 1) lamptab = argv[1];
	if (loadlamps(lamptab) == 0) {
		perror(lamptab);
		exit(1);
	}
	for ( ; ; ) {
		printf("Enter lamp type: ");
		buf[0] = '\0';
		gets(buf);
		if (!buf[0]) exit(0);
		if ((lcol = matchlamp(buf)) == NULL)
			printf("No match in table\n");
		else
			printf("RGB = (%f,%f,%f)\n", lcol[0], lcol[1], lcol[2]);
	}
}
