/* Copyright (c) 1991 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  lookamb.c - program to examine ambient components.
 *
 *     10/8/86
 */

#include  <stdio.h>

#include  "color.h"


typedef struct ambval {
	float  pos[3];		/* position in space */
	float  dir[3];		/* normal direction */
	int  lvl;		/* recursion level of parent ray */
	float  weight;		/* weight of parent ray */
	COLOR  val;		/* computed ambient value */
	float  rad;		/* validity radius */
	struct ambval  *next;	/* next in list */
}  AMBVAL;			/* ambient value */

int  dataonly = 0;

int  reverse = 0;

AMBVAL  av;


main(argc, argv)		/* load ambient values from a file */
char  *argv[];
{
	FILE  *fp;
	int  i;

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'd':
				dataonly = 1;
				break;
			case 'r':
				reverse = 1;
				break;
			default:
				fprintf(stderr, "%s: unknown option '%s'\n",
						argv[0], argv[i]);
				return(1);
			}
		else
			break;

	if (i >= argc)
		fp = stdin;
	else if ((fp = fopen(argv[i], "r")) == NULL) {
		fprintf(stderr, "%s: file not found\n", argv[i]);
		return(1);
	}
	if (reverse)
		writamb(fp);
	else
		lookamb(fp);
	fclose(fp);
	return(0);
}


lookamb(fp)			/* get ambient values from a file */
FILE  *fp;
{
	while (fread((char *)&av, sizeof(AMBVAL), 1, fp) == 1) {
		if (dataonly) {
			printf("%f\t%f\t%f\t", av.pos[0], av.pos[1], av.pos[2]);
			printf("%f\t%f\t%f\t", av.dir[0], av.dir[1], av.dir[2]);
			printf("%d\t%f\t%f\t", av.lvl, av.weight, av.rad);
			printf("%e\t%e\t%e\n", colval(av.val,RED),
						colval(av.val,GRN),
						colval(av.val,BLU));
		} else {
			printf("\nPosition:\t%f\t%f\t%f\n", av.pos[0],
					av.pos[1], av.pos[2]);
			printf("Direction:\t%f\t%f\t%f\n", av.dir[0],
					av.dir[1], av.dir[2]);
			printf("Lvl,Wt,Rad:\t%d\t\t%f\t%f\n", av.lvl,
					av.weight, av.rad);
			printf("Value:\t\t%e\t%e\t%e\n", colval(av.val,RED),
					colval(av.val,GRN), colval(av.val,BLU));
		}
		if (ferror(stdout))
			exit(1);
	}
}


writamb(fp)			/* write binary ambient values */
FILE  *fp;
{
	for ( ; ; ) {
		if (!dataonly)
			fscanf(fp, "%*s");
		if (fscanf(fp, "%f %f %f",
				&av.pos[0], &av.pos[1], &av.pos[2]) != 3)
			return;
		if (!dataonly)
			fscanf(fp, "%*s");
		if (fscanf(fp, "%f %f %f",
				&av.dir[0], &av.dir[1], &av.dir[2]) != 3)
			return;
		if (!dataonly)
			fscanf(fp, "%*s");
		if (fscanf(fp, "%d %f %f",
				&av.lvl, &av.weight, &av.rad) != 3)
			return;
		if (!dataonly)
			fscanf(fp, "%*s");
		if (fscanf(fp, "%f %f %f",
				&av.val[RED], &av.val[GRN], &av.val[BLU]) != 3)
			return;
		fwrite((char *)&av, sizeof(AMBVAL), 1, stdout);
		if (ferror(stdout))
			exit(1);
	}
}
