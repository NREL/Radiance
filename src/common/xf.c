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


int
xf(xfmat, xfsca, ac, av)		/* get transform specification */
double  xfmat[4][4];
double  *xfsca;
int  ac;
char  *av[];
{
	double  atof(), sin(), cos();
	double  m4[4][4];
	double  theta;
	int  i;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			m4[3][0] = atof(av[++i]);
			m4[3][1] = atof(av[++i]);
			m4[3][2] = atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				theta = PI/180.0 * atof(av[++i]);
				m4[1][1] = m4[2][2] = cos(theta);
				m4[2][1] = -(m4[1][2] = sin(theta));
				break;
			case 'y':
				theta = PI/180 * atof(av[++i]);
				m4[0][0] = m4[2][2] = cos(theta);
				m4[0][2] = -(m4[2][0] = sin(theta));
				break;
			case 'z':
				theta = PI/180 * atof(av[++i]);
				m4[0][0] = m4[1][1] = cos(theta);
				m4[1][0] = -(m4[0][1] = sin(theta));
				break;
			default:
				return(i);
			}
			break;

		case 's':			/* scale */
			*xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = atof(av[++i]);
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				*xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				*xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				*xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				return(i);
			}
			break;

		default:
			return(i);

		}
		multmat4(xfmat, xfmat, m4);
	}
	return(i);
}


#ifdef  INVXF
int
invxf(xfmat, xfsca, ac, av)		/* invert transform specification */
double  xfmat[4][4];
double  *xfsca;
int  ac;
char  *av[];
{
	double  atof(), sin(), cos();
	double  m4[4][4];
	double  theta;
	int  i;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			m4[3][0] = -atof(av[++i]);
			m4[3][1] = -atof(av[++i]);
			m4[3][2] = -atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				theta = -PI/180.0 * atof(av[++i]);
				m4[1][1] = m4[2][2] = cos(theta);
				m4[2][1] = -(m4[1][2] = sin(theta));
				break;
			case 'y':
				theta = -PI/180.0 * atof(av[++i]);
				m4[0][0] = m4[2][2] = cos(theta);
				m4[0][2] = -(m4[2][0] = sin(theta));
				break;
			case 'z':
				theta = -PI/180.0 * atof(av[++i]);
				m4[0][0] = m4[1][1] = cos(theta);
				m4[1][0] = -(m4[0][1] = sin(theta));
				break;
			default:
				return(i);
			}
			break;

		case 's':			/* scale */
			*xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = 1.0 / atof(av[++i]);
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				*xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				*xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				*xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				return(i);
			}
			break;

		default:
			return(i);

		}
		multmat4(xfmat, m4, xfmat);	/* left multiply */
	}
	return(i);
}
#endif
