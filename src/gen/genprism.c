/* Copyright (c) 1987 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  genprism.c - generate a prism.
 *		2D vertices in the xy plane are given on the
 *		command line or from a file.  Their order together
 *		with the extrude direction will determine surface
 *		orientation.
 *
 *     8/24/87
 */

#include  <stdio.h>

#include  <ctype.h>

#define  MAXVERT	1024		/* maximum # vertices */

#ifndef atof
extern double  atof();
#endif

char  *pmtype;		/* material type */
char  *pname;		/* name */

double  lvect[3] = {0.0, 0.0, 1.0};

double  vert[MAXVERT][2];
int  nverts = 0;

int  do_ends = 1;		/* include end caps */
int  iscomplete = 0;		/* polygon is already completed */


main(argc, argv)
int  argc;
char  **argv;
{
	int  an;
	
	if (argc < 4)
		goto userr;

	pmtype = argv[1];
	pname = argv[2];

	if (!strcmp(argv[3], "-")) {
		readverts(NULL);
		an = 4;
	} else if (isdigit(argv[3][0])) {
		nverts = atoi(argv[3]);
		if (argc-3 < 2*nverts)
			goto userr;
		for (an = 0; an < nverts; an++) {
			vert[an][0] = atof(argv[2*an+4]);
			vert[an][1] = atof(argv[2*an+5]);
		}
		an = 2*nverts+4;
	} else {
		readverts(argv[3]);
		an = 4;
	}
	if (nverts < 3) {
		fprintf(stderr, "%s: not enough vertices\n", argv[0]);
		exit(1);
	}

	for ( ; an < argc; an++) {
		if (argv[an][0] != '-')
			goto userr;
		switch (argv[an][1]) {
		case 'l':				/* length vector */
			lvect[0] = atof(argv[++an]);
			lvect[1] = atof(argv[++an]);
			lvect[2] = atof(argv[++an]);
			break;
		case 'e':				/* ends */
			do_ends = !do_ends;
			break;
		case 'c':				/* complete */
			iscomplete = !iscomplete;
			break;
		default:
			goto userr;
		}
	}

	printhead(argc, argv);

	if (do_ends)
		printends();
	printsides();

	return(0);
userr:
	fprintf(stderr, "Usage: %s material name ", argv[0]);
	fprintf(stderr, "{ - | vfile | N v1 v2 .. vN } ");
	fprintf(stderr, "[-l lvect][-c][-e]\n");
	exit(1);
}


readverts(fname)		/* read vertices from a file */
char  *fname;
{
	FILE  *fp;

	if (fname == NULL)
		fp = stdin;
	else if ((fp = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open\n", fname);
		exit(1);
	}
	while (fscanf(fp, "%lf %lf", &vert[nverts][0], &vert[nverts][1]) == 2)
		nverts++;
	fclose(fp);
}


printends()			/* print ends of prism */
{
	register int  i;

	printf("\n%s polygon %s.b\n", pmtype, pname);
	printf("0\n0\n%d\n", nverts*3);
	for (i = 0; i < nverts; i++) {
		printf("\t%18.12g\t%18.12g\t%18.12g\n",
				vert[i][0],
				vert[i][1],
				0.0);
	}
	printf("\n%s polygon %s.e\n", pmtype, pname);
	printf("0\n0\n%d\n", nverts*3);
	for (i = nverts-1; i >= 0; i--) {
		printf("\t%18.12g\t%18.12g\t%18.12g\n",
				vert[i][0]+lvect[0],
				vert[i][1]+lvect[1],
				lvect[2]);
	}
}


printsides()			/* print prism sides */
{
	register int  i;

	for (i = 0; i < nverts-1; i++)
		side(i, i+1);
	if (!iscomplete)
		side(nverts-1, 0);
}


side(n1, n2)			/* print single side */
register int  n1, n2;
{
	printf("\n%s polygon %s.%d\n", pmtype, pname, n1+1);
	printf("0\n0\n12\n");
	printf("\t%18.12g\t%18.12g\t%18.12g\n",
			vert[n1][0],
			vert[n1][1],
			0.0);
	printf("\t%18.12g\t%18.12g\t%18.12g\n",
			vert[n1][0]+lvect[0],
			vert[n1][1]+lvect[1],
			lvect[2]);
	printf("\t%18.12g\t%18.12g\t%18.12g\n",
			vert[n2][0]+lvect[0],
			vert[n2][1]+lvect[1],
			lvect[2]);
	printf("\t%18.12g\t%18.12g\t%18.12g\n",
			vert[n2][0],
			vert[n2][1],
			0.0);
}


printhead(ac, av)		/* print command header */
register int  ac;
register char  **av;
{
	putchar('#');
	while (ac--) {
		putchar(' ');
		fputs(*av++, stdout);
	}
	putchar('\n');
}
