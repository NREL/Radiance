/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  xf.c - routines to convert transform arguments into 4X4 matrix.
 *
 *     1/28/86
 */


#define  PI		3.14159265358979323846

#define  checkarg(a,n)	if (strcmp(av[i],a) || i+n >= ac) goto done


int
xf(retmat, retsca, ac, av)		/* get transform specification */
double  retmat[4][4];
double  *retsca;
int  ac;
char  *av[];
{
	double  atof(), sin(), cos();
	double  xfmat[4][4], m4[4][4];
	double  xfsca, theta;
	int  i, j;
	int  rept;

	setident4(retmat);
	*retsca = 1.0;

	rept = 1;
	setident4(xfmat);
	xfsca = 1.0;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			checkarg("-t",3);
			m4[3][0] = atof(av[++i]);
			m4[3][1] = atof(av[++i]);
			m4[3][2] = atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				checkarg("-rx",1);
				theta = PI/180.0 * atof(av[++i]);
				m4[1][1] = m4[2][2] = cos(theta);
				m4[2][1] = -(m4[1][2] = sin(theta));
				break;
			case 'y':
				checkarg("-ry",1);
				theta = PI/180.0 * atof(av[++i]);
				m4[0][0] = m4[2][2] = cos(theta);
				m4[0][2] = -(m4[2][0] = sin(theta));
				break;
			case 'z':
				checkarg("-rz",1);
				theta = PI/180.0 * atof(av[++i]);
				m4[0][0] = m4[1][1] = cos(theta);
				m4[1][0] = -(m4[0][1] = sin(theta));
				break;
			default:
				return(i);
			}
			break;

		case 's':			/* scale */
			checkarg("-s",1);
			xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = atof(av[++i]);
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				checkarg("-mx",0);
				xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				checkarg("-my",0);
				xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				checkarg("-mz",0);
				xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				return(i);
			}
			break;

		case 'i':			/* iterate */
			checkarg("-i",1);
			for (j = 0; j < rept; j++) {
				multmat4(retmat, retmat, xfmat);
				*retsca *= xfsca;
			}
			rept = atoi(av[++i]);
			setident4(xfmat);
			xfsca = 1.0;
			break;

		default:
			return(i);

		}
		multmat4(xfmat, xfmat, m4);
	}
done:
	for (j = 0; j < rept; j++) {
		multmat4(retmat, retmat, xfmat);
		*retsca *= xfsca;
	}
	return(i);
}


#ifdef  INVXF
int
invxf(retmat, retsca, ac, av)		/* invert transform specification */
double  retmat[4][4];
double  *retsca;
int  ac;
char  *av[];
{
	double  atof(), sin(), cos();
	double  xfmat[4][4], m4[4][4];
	double  xfsca, theta;
	int  i, j;
	int  rept;

	setident4(retmat);
	*retsca = 1.0;

	rept = 1;
	setident4(xfmat);
	xfsca = 1.0;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			checkarg("-t",3);
			m4[3][0] = -atof(av[++i]);
			m4[3][1] = -atof(av[++i]);
			m4[3][2] = -atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				checkarg("-rx",1);
				theta = -PI/180.0 * atof(av[++i]);
				m4[1][1] = m4[2][2] = cos(theta);
				m4[2][1] = -(m4[1][2] = sin(theta));
				break;
			case 'y':
				checkarg("-ry",1);
				theta = -PI/180.0 * atof(av[++i]);
				m4[0][0] = m4[2][2] = cos(theta);
				m4[0][2] = -(m4[2][0] = sin(theta));
				break;
			case 'z':
				checkarg("-rz",1);
				theta = -PI/180.0 * atof(av[++i]);
				m4[0][0] = m4[1][1] = cos(theta);
				m4[1][0] = -(m4[0][1] = sin(theta));
				break;
			default:
				return(i);
			}
			break;

		case 's':			/* scale */
			checkarg("-s",1);
			*xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = 1.0 / atof(av[++i]);
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				checkarg("-mx",0);
				*xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				checkarg("-my",0);
				*xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				checkarg("-mz",0);
				*xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				return(i);
			}
			break;

		case 'i':			/* iterate */
			checkarg("-i",1);
			for (j = 0; j < rept; j++) {
				multmat4(retmat, xfmat, retmat);
				*retsca *= xfsca;
			}
			rept = atoi(av[++i]);
			setident4(xfmat);
			xfsca = 1.0;
			break;

		default:
			return(i);

		}
		multmat4(xfmat, m4, xfmat);	/* left multiply */
	}
done:
	for (j = 0; j < rept; j++) {
		multmat4(retmat, xfmat, retmat);
		*retsca *= xfsca;
	}
	return(i);
}
#endif
