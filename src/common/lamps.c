/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Load lamp data.
 */

#include  <stdio.h>
#include  <ctype.h>

extern char	*eindex(), *expsave(), *malloc();
extern FILE	*fropen();

typedef struct lamp {
	char	*pattern;			/* search pattern */
	float	*color;				/* pointer to lamp value */
	struct lamp	*next;			/* next lamp in list */
} LAMP;					/* a lamp entry */

static LAMP	*lamps = NULL;		/* lamp list */


float *
matchlamp(s)				/* see if string matches any lamp */
char	*s;
{
	register LAMP	*lp;

	for (lp = lamps; lp != NULL; lp = lp->next) {
		expset(lp->pattern);
		if (eindex(s) != NULL)
			return(lp->color);
	}
	return(NULL);
}


loadlamps(file)					/* load lamp type file */
char	*file;
{
	LAMP	*lastp;
	register LAMP	*lp;
	FILE	*fp;
	float	xyz[3];
	char	buf[128], str[128];
	register char	*cp1, *cp2;

	if ((fp = fropen(file)) == NULL)
		return(0);
	lastp = NULL;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
					/* work on a copy of buffer */
		strcpy(str, buf);
					/* get pattern for this entry */
		for (cp1 = str; isspace(*cp1); cp1++)
			;
		if (!*cp1 || *cp1 == '#')
			continue;
		for (cp2 = cp1+1; *cp2; cp2++)
			if (*cp2 == *cp1 && cp2[-1] != '\\')
				break;
		if (!*cp2) {
			cp1 = "unclosed pattern";
			goto fmterr;
		}
		cp1++; *cp2++ = '\0';
		if (ecompile(cp1, 1, 0) < 0) {
			cp1 = "bad regular expression";
			goto fmterr;
		}
		if ((lp = (LAMP *)malloc(sizeof(LAMP))) == NULL)
			goto memerr;
		if ((lp->pattern = expsave()) == NULL)
			goto memerr;
						/* get specification */
		for (cp1 = cp2; isspace(*cp1); cp1++)
			;
		if (!isdigit(*cp1) && *cp1 != '.' && *cp1 != '(') {
			cp1 = "missing lamp specification";
			goto fmterr;
		}
		if (*cp1 == '(') {		/* find alias */
			for (cp2 = ++cp1; *cp2; cp2++)
				if (*cp2 == ')' && cp2[-1] != '\\')
					break;
			*cp2 = '\0';
			if ((lp->color = matchlamp(cp1)) == NULL) {
				cp1 = "unmatched alias";
				goto fmterr;
			}
		} else {			/* or read specificaion */
			if ((lp->color=(float *)malloc(3*sizeof(float)))==NULL)
				goto memerr;
			if (sscanf(cp1, "%f %f %f", &lp->color[0], &
					lp->color[1], &lp->color[2]) != 3) {
				cp1 = "bad lamp data";
				goto fmterr;
			}
						/* convert xyY to XYZ */
			xyz[1] = lp->color[2];
			xyz[0] = lp->color[0]/lp->color[1] * xyz[1];
			xyz[2] = xyz[1]*(1./lp->color[1] - 1.) - xyz[0];
						/* XYZ to RGB */
			cie_rgb(lp->color, xyz);
		}
		if (lastp == NULL)
			lamps = lp;
		else
			lastp->next = lp;
		lp->next = NULL;
		lastp = lp;
	}
	fclose(fp);
	return(1);
memerr:
	fputs("Out of memory in loadlamps\n", stderr);
	return(-1);
fmterr:
	fputs(buf, stderr);
	fprintf(stderr, "%s: %s\n", file, cp1);
	return(-1);
}
