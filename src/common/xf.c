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
#define  d2r(a)		((PI/180.)*(a))

#define  checkarg(a,n)	if (av[i][a] || i+n >= ac) goto done


int
xf(retmat, retsca, ac, av)		/* get transform specification */
double  retmat[4][4];
double  *retsca;
int  ac;
char  *av[];
{
	double  atof(), sin(), cos();
	double  xfmat[4][4], m4[4][4];
	double  xfsca, dtmp;
	int  i, icnt;

	setident4(retmat);
	*retsca = 1.0;

	setident4(xfmat);
	xfsca = 1.0;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			checkarg(2,3);
			m4[3][0] = atof(av[++i]);
			m4[3][1] = atof(av[++i]);
			m4[3][2] = atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,1);
				dtmp = d2r(atof(av[++i]));
				m4[1][1] = m4[2][2] = cos(dtmp);
				m4[2][1] = -(m4[1][2] = sin(dtmp));
				break;
			case 'y':
				checkarg(3,1);
				dtmp = d2r(atof(av[++i]));
				m4[0][0] = m4[2][2] = cos(dtmp);
				m4[0][2] = -(m4[2][0] = sin(dtmp));
				break;
			case 'z':
				checkarg(3,1);
				dtmp = d2r(atof(av[++i]));
				m4[0][0] = m4[1][1] = cos(dtmp);
				m4[1][0] = -(m4[0][1] = sin(dtmp));
				break;
			default:
				return(i);
			}
			break;

		case 's':			/* scale */
			checkarg(2,1);
			dtmp = atof(av[i+1]);
			if (dtmp == 0.0) goto done;
			i++;
			xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = dtmp;
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,0);
				xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				checkarg(3,0);
				xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				checkarg(3,0);
				xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				return(i);
			}
			break;

		case 'i':			/* iterate */
			checkarg(2,1);
			icnt = atoi(av[++i]);
			while (icnt-- > 0) {
				multmat4(retmat, retmat, xfmat);
				*retsca *= xfsca;
			}
			setident4(xfmat);
			xfsca = 1.0;
			break;

		default:
			return(i);

		}
		multmat4(xfmat, xfmat, m4);
	}
done:
	multmat4(retmat, retmat, xfmat);
	*retsca *= xfsca;
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
	double  xfsca, dtmp;
	int  i, icnt;

	setident4(retmat);
	*retsca = 1.0;

	setident4(xfmat);
	xfsca = 1.0;

	for (i = 0; i < ac && av[i][0] == '-'; i++) {
	
		setident4(m4);
		
		switch (av[i][1]) {
	
		case 't':			/* translate */
			checkarg(2,3);
			m4[3][0] = -atof(av[++i]);
			m4[3][1] = -atof(av[++i]);
			m4[3][2] = -atof(av[++i]);
			break;

		case 'r':			/* rotate */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,1);
				dtmp = -d2r(atof(av[++i]));
				m4[1][1] = m4[2][2] = cos(dtmp);
				m4[2][1] = -(m4[1][2] = sin(dtmp));
				break;
			case 'y':
				checkarg(3,1);
				dtmp = -d2r(atof(av[++i]));
				m4[0][0] = m4[2][2] = cos(dtmp);
				m4[0][2] = -(m4[2][0] = sin(dtmp));
				break;
			case 'z':
				checkarg(3,1);
				dtmp = -d2r(atof(av[++i]));
				m4[0][0] = m4[1][1] = cos(dtmp);
				m4[1][0] = -(m4[0][1] = sin(dtmp));
				break;
			default:
				return(i);
			}
			break;

		case 's':			/* scale */
			checkarg(2,1);
			dtmp = atof(av[i+1]);
			if (dtmp == 0.0) goto done;
			i++;
			xfsca *=
			m4[0][0] = 
			m4[1][1] = 
			m4[2][2] = 1.0 / dtmp;
			break;

		case 'm':			/* mirror */
			switch (av[i][2]) {
			case 'x':
				checkarg(3,0);
				xfsca *=
				m4[0][0] = -1.0;
				break;
			case 'y':
				checkarg(3,0);
				xfsca *=
				m4[1][1] = -1.0;
				break;
			case 'z':
				checkarg(3,0);
				xfsca *=
				m4[2][2] = -1.0;
				break;
			default:
				return(i);
			}
			break;

		case 'i':			/* iterate */
			checkarg(2,1);
			icnt = atoi(av[++i]);
			while (icnt-- > 0) {
				multmat4(retmat, xfmat, retmat);
				*retsca *= xfsca;
			}
			setident4(xfmat);
			xfsca = 1.0;
			break;

		default:
			return(i);

		}
		multmat4(xfmat, m4, xfmat);	/* left multiply */
	}
done:
	multmat4(retmat, xfmat, retmat);
	*retsca *= xfsca;
	return(i);
}
#endif
