#ifndef lint
static const char RCSid[] = "$Id: rmatrix.c,v 2.44 2020/05/07 18:45:16 greg Exp $";
#endif
/*
 * General matrix operations.
 */

#include <stdlib.h>
#include <errno.h>
#include "rtio.h"
#include "platform.h"
#include "resolu.h"
#include "paths.h"
#include "rmatrix.h"

static char	rmx_mismatch_warn[] = "WARNING: data type mismatch\n";

/* Allocate a nr x nc matrix with n components */
RMATRIX *
rmx_alloc(int nr, int nc, int n)
{
	RMATRIX	*dnew;

	if ((nr <= 0) | (nc <= 0) | (n <= 0))
		return(NULL);
	dnew = (RMATRIX *)malloc(sizeof(RMATRIX)-sizeof(dnew->mtx) +
					sizeof(dnew->mtx[0])*(n*nr*nc));
	if (!dnew)
		return(NULL);
	dnew->nrows = nr; dnew->ncols = nc; dnew->ncomp = n;
	dnew->dtype = DTdouble;
	dnew->swapin = 0;
	dnew->info = NULL;
	return(dnew);
}

/* Free a RMATRIX array */
void
rmx_free(RMATRIX *rm)
{
	if (!rm) return;
	if (rm->info)
		free(rm->info);
	free(rm);
}

/* Resolve data type based on two input types (returns 0 for mismatch) */
int
rmx_newtype(int dtyp1, int dtyp2)
{
	if ((dtyp1==DTxyze) | (dtyp1==DTrgbe) |
			(dtyp2==DTxyze) | (dtyp2==DTrgbe)
			&& dtyp1 != dtyp2)
		return(0);
	if (dtyp1 < dtyp2)
		return(dtyp1);
	return(dtyp2);
}

/* Append header information associated with matrix data */
int
rmx_addinfo(RMATRIX *rm, const char *info)
{
	if (!rm || !info || !*info)
		return(0);
	if (!rm->info) {
		rm->info = (char *)malloc(strlen(info)+1);
		if (rm->info) rm->info[0] = '\0';
	} else
		rm->info = (char *)realloc(rm->info,
				strlen(rm->info)+strlen(info)+1);
	if (!rm->info)
		return(0);
	strcat(rm->info, info);
	return(1);
}

static int
get_dminfo(char *s, void *p)
{
	RMATRIX	*ip = (RMATRIX *)p;
	char	fmt[MAXFMTLEN];
	int	i;

	if (headidval(fmt, s))
		return(0);
	if (!strncmp(s, "NCOMP=", 6)) {
		ip->ncomp = atoi(s+6);
		return(0);
	}
	if (!strncmp(s, "NROWS=", 6)) {
		ip->nrows = atoi(s+6);
		return(0);
	}
	if (!strncmp(s, "NCOLS=", 6)) {
		ip->ncols = atoi(s+6);
		return(0);
	}
	if ((i = isbigendian(s)) >= 0) {
		ip->swapin = (nativebigendian() != i);
		return(0);
	}
	if (isexpos(s)) {
		double	d = exposval(s);
		scalecolor(ip->mtx, d);
		return(0);
	}
	if (iscolcor(s)) {
		COLOR	ctmp;
		colcorval(ctmp, s);
		multcolor(ip->mtx, ctmp);
		return(0);
	}
	if (!formatval(fmt, s)) {
		rmx_addinfo(ip, s);
		return(0);
	}
	for (i = 1; i < DTend; i++)
		if (!strcmp(fmt, cm_fmt_id[i])) {
			ip->dtype = i;
			return(0);
		}
	return(-1);
}

static int
rmx_load_ascii(RMATRIX *rm, FILE *fp)
{
	int	i, j, k;

	for (i = 0; i < rm->nrows; i++)
	    for (j = 0; j < rm->ncols; j++)
	        for (k = 0; k < rm->ncomp; k++)
		    if (fscanf(fp, "%lf", &rmx_lval(rm,i,j,k)) != 1)
			return(0);
	return(1);
}

static int
rmx_load_float(RMATRIX *rm, FILE *fp)
{
	int	i, j, k;
	float	val[100];

	if (rm->ncomp > 100) {
		fputs("Unsupported # components in rmx_load_float()\n", stderr);
		exit(1);
	}
	for (i = 0; i < rm->nrows; i++)
	    for (j = 0; j < rm->ncols; j++) {
		if (getbinary(val, sizeof(val[0]), rm->ncomp, fp) != rm->ncomp)
		    return(0);
		if (rm->swapin)
		    swap32((char *)val, rm->ncomp);
	        for (k = rm->ncomp; k--; )
		     rmx_lval(rm,i,j,k) = val[k];
	    }
	return(1);
}

static int
rmx_load_double(RMATRIX *rm, FILE *fp)
{
	int	i;

	if ((char *)&rmx_lval(rm,1,0,0) - (char *)&rmx_lval(rm,0,0,0) !=
					sizeof(double)*rm->ncols*rm->ncomp) {
		fputs("Code error in rmx_load_double()\n", stderr);
		exit(1);
	}
	for (i = 0; i < rm->nrows; i++) {
		if (getbinary(&rmx_lval(rm,i,0,0), sizeof(double)*rm->ncomp,
					rm->ncols, fp) != rm->ncols)
			return(0);
		if (rm->swapin)
			swap64((char *)&rmx_lval(rm,i,0,0), rm->ncols*rm->ncomp);
	}
	return(1);
}

static int
rmx_load_rgbe(RMATRIX *rm, FILE *fp)
{
	COLOR	*scan = (COLOR *)malloc(sizeof(COLOR)*rm->ncols);
	int	i, j;

	if (!scan)
		return(0);
	for (i = 0; i < rm->nrows; i++) {
	    if (freadscan(scan, rm->ncols, fp) < 0) {
		free(scan);
		return(0);
	    }
	    for (j = rm->ncols; j--; ) {
	        rmx_lval(rm,i,j,0) = colval(scan[j],RED);
	        rmx_lval(rm,i,j,1) = colval(scan[j],GRN);
	        rmx_lval(rm,i,j,2) = colval(scan[j],BLU);
	    }
	}
	free(scan);
	return(1);
}

/* Load matrix from supported file type */
RMATRIX *
rmx_load(const char *inspec)
{
	FILE		*fp = stdin;
	RMATRIX		dinfo;
	RMATRIX		*dnew;

	if (!inspec) {				/* reading from stdin? */
		inspec = "<stdin>";
		SET_FILE_BINARY(stdin);
	} else if (inspec[0] == '!') {
		if (!(fp = popen(inspec+1, "r")))
			return(NULL);
		SET_FILE_BINARY(fp);
	} else {
		const char	*sp = inspec;	/* check suffix */
		while (*sp)
			++sp;
		while (sp > inspec && sp[-1] != '.')
			--sp;
		if (!strcasecmp(sp, "XML")) {	/* assume it's a BSDF */
			CMATRIX	*cm = cm_loadBTDF((char *)inspec);
			if (!cm)
				return(NULL);
			dnew = rmx_from_cmatrix(cm);
			cm_free(cm);
			dnew->dtype = DTascii;
			return(dnew);
		}
						/* else open it ourselves */
		if (!(fp = fopen(inspec, "rb")))
			return(NULL);
	}
#ifdef getc_unlocked
	flockfile(fp);
#endif
	dinfo.nrows = dinfo.ncols = dinfo.ncomp = 0;
	dinfo.dtype = DTascii;			/* assumed w/o FORMAT */
	dinfo.swapin = 0;
	dinfo.info = NULL;
	dinfo.mtx[0] = dinfo.mtx[1] = dinfo.mtx[2] = 1.;
	if (getheader(fp, get_dminfo, &dinfo) < 0) {
		fclose(fp);
		return(NULL);
	}
	if ((dinfo.nrows <= 0) | (dinfo.ncols <= 0)) {
		if (!fscnresolu(&dinfo.ncols, &dinfo.nrows, fp)) {
			fclose(fp);
			return(NULL);
		}
		if (dinfo.ncomp <= 0)
			dinfo.ncomp = 3;
		else if ((dinfo.dtype == DTrgbe) | (dinfo.dtype == DTxyze) &&
				dinfo.ncomp != 3) {
			fclose(fp);
			return(NULL);
		}
	}
	dnew = rmx_alloc(dinfo.nrows, dinfo.ncols, dinfo.ncomp);
	if (!dnew) {
		fclose(fp);
		return(NULL);
	}
	dnew->info = dinfo.info;
	switch (dinfo.dtype) {
	case DTascii:
		SET_FILE_TEXT(fp);
		if (!rmx_load_ascii(dnew, fp))
			goto loaderr;
		dnew->dtype = DTascii;		/* should leave double? */
		break;
	case DTfloat:
		dnew->swapin = dinfo.swapin;
		if (!rmx_load_float(dnew, fp))
			goto loaderr;
		dnew->dtype = DTfloat;
		break;
	case DTdouble:
		dnew->swapin = dinfo.swapin;
		if (!rmx_load_double(dnew, fp))
			goto loaderr;
		dnew->dtype = DTdouble;
		break;
	case DTrgbe:
	case DTxyze:
		if (!rmx_load_rgbe(dnew, fp))
			goto loaderr;
		dnew->dtype = dinfo.dtype;	/* undo exposure? */
		if ((dinfo.mtx[0] != 1.) | (dinfo.mtx[1] != 1.) |
				(dinfo.mtx[2] != 1.)) {
			dinfo.mtx[0] = 1./dinfo.mtx[0];
			dinfo.mtx[1] = 1./dinfo.mtx[1];
			dinfo.mtx[2] = 1./dinfo.mtx[2];
			rmx_scale(dnew, dinfo.mtx);
		}
		break;
	default:
		goto loaderr;
	}
	if (fp != stdin) {
		if (inspec[0] == '!')
			pclose(fp);
		else
			fclose(fp);
	}
#ifdef getc_unlocked
	else
		funlockfile(fp);
#endif
	return(dnew);
loaderr:					/* should report error? */
	if (inspec[0] == '!')
		pclose(fp);
	else
		fclose(fp);
	rmx_free(dnew);
	return(NULL);
}

static int
rmx_write_ascii(const RMATRIX *rm, FILE *fp)
{
	const char	*fmt = (rm->dtype == DTfloat) ? " %.7e" :
			(rm->dtype == DTrgbe) | (rm->dtype == DTxyze) ? " %.3e" :
				" %.15e" ;
	int	i, j, k;

	for (i = 0; i < rm->nrows; i++) {
	    for (j = 0; j < rm->ncols; j++) {
	        for (k = 0; k < rm->ncomp; k++)
		    fprintf(fp, fmt, rmx_lval(rm,i,j,k));
		fputc('\t', fp);
	    }
	    fputc('\n', fp);
	}
	return(1);
}

static int
rmx_write_float(const RMATRIX *rm, FILE *fp)
{
	int	i, j, k;
	float	val[100];

	if (rm->ncomp > 100) {
		fputs("Unsupported # components in rmx_write_float()\n", stderr);
		exit(1);
	}
	for (i = 0; i < rm->nrows; i++)
	    for (j = 0; j < rm->ncols; j++) {
	        for (k = rm->ncomp; k--; )
		    val[k] = (float)rmx_lval(rm,i,j,k);
		if (putbinary(val, sizeof(val[0]), rm->ncomp, fp) != rm->ncomp)
			return(0);
	    }
	return(1);
}

static int
rmx_write_double(const RMATRIX *rm, FILE *fp)
{
	int	i, j;

	for (i = 0; i < rm->nrows; i++)
	    for (j = 0; j < rm->ncols; j++)
		if (putbinary(&rmx_lval(rm,i,j,0), sizeof(double), rm->ncomp, fp) != rm->ncomp)
			return(0);
	return(1);
}

static int
rmx_write_rgbe(const RMATRIX *rm, FILE *fp)
{
	COLR	*scan = (COLR *)malloc(sizeof(COLR)*rm->ncols);
	int	i, j;

	if (!scan)
		return(0);
	for (i = 0; i < rm->nrows; i++) {
	    for (j = rm->ncols; j--; )
	        setcolr(scan[j],	rmx_lval(rm,i,j,0),
					rmx_lval(rm,i,j,1),
					rmx_lval(rm,i,j,2)	);
	    if (fwritecolrs(scan, rm->ncols, fp) < 0) {
		free(scan);
		return(0);
	    }
	}
	free(scan);
	return(1);
}

/* Write matrix to file type indicated by dtype */
int
rmx_write(const RMATRIX *rm, int dtype, FILE *fp)
{
	RMATRIX	*mydm = NULL;
	int	ok = 1;

	if (!rm | !fp)
		return(0);
#ifdef getc_unlocked
	flockfile(fp);
#endif
						/* complete header */
	if (rm->info)
		fputs(rm->info, fp);
	if (dtype == DTfromHeader)
		dtype = rm->dtype;
	else if ((dtype == DTrgbe) & (rm->dtype == DTxyze))
		dtype = DTxyze;
	else if ((dtype == DTxyze) & (rm->dtype == DTrgbe))
		dtype = DTrgbe;
	if ((dtype != DTrgbe) & (dtype != DTxyze)) {
		fprintf(fp, "NROWS=%d\n", rm->nrows);
		fprintf(fp, "NCOLS=%d\n", rm->ncols);
		fprintf(fp, "NCOMP=%d\n", rm->ncomp);
	} else if (rm->ncomp != 3) {		/* wrong # components? */
		double	cmtx[3];
		if (rm->ncomp != 1)		/* only convert grayscale */
			return(0);
		cmtx[0] = cmtx[1] = cmtx[2] = 1;
		mydm = rmx_transform(rm, 3, cmtx);
		if (!mydm)
			return(0);
		rm = mydm;
	}
	if ((dtype == DTfloat) | (dtype == DTdouble))
		fputendian(fp);			/* important to record */
	fputformat((char *)cm_fmt_id[dtype], fp);
	fputc('\n', fp);
	switch (dtype) {			/* write data */
	case DTascii:
		ok = rmx_write_ascii(rm, fp);
		break;
	case DTfloat:
		ok = rmx_write_float(rm, fp);
		break;
	case DTdouble:
		ok = rmx_write_double(rm, fp);
		break;
	case DTrgbe:
	case DTxyze:
		fprtresolu(rm->ncols, rm->nrows, fp);
		ok = rmx_write_rgbe(rm, fp);
		break;
	default:
		return(0);
	}
	ok &= (fflush(fp) == 0);
#ifdef getc_unlocked
	funlockfile(fp);
#endif
	if (mydm)
		rmx_free(mydm);
	return(ok);
}

/* Allocate and assign square identity matrix with n components */
RMATRIX *
rmx_identity(const int dim, const int n)
{
	RMATRIX	*rid = rmx_alloc(dim, dim, n);
	int	i, k;

	if (!rid)
		return(NULL);
	memset(rid->mtx, 0, sizeof(rid->mtx[0])*n*dim*dim);
	for (i = dim; i--; )
	    for (k = n; k--; )
		rmx_lval(rid,i,i,k) = 1;
	return(rid);
}

/* Duplicate the given matrix */
RMATRIX *
rmx_copy(const RMATRIX *rm)
{
	RMATRIX	*dnew;

	if (!rm)
		return(NULL);
	dnew = rmx_alloc(rm->nrows, rm->ncols, rm->ncomp);
	if (!dnew)
		return(NULL);
	rmx_addinfo(dnew, rm->info);
	dnew->dtype = rm->dtype;
	memcpy(dnew->mtx, rm->mtx,
		sizeof(rm->mtx[0])*rm->ncomp*rm->nrows*rm->ncols);
	return(dnew);
}

/* Allocate and assign transposed matrix */
RMATRIX *
rmx_transpose(const RMATRIX *rm)
{
	RMATRIX	*dnew;
	int	i, j, k;

	if (!rm)
		return(0);
	if ((rm->nrows == 1) | (rm->ncols == 1)) {
		dnew = rmx_copy(rm);
		if (!dnew)
			return(NULL);
		dnew->nrows = rm->ncols;
		dnew->ncols = rm->nrows;
		return(dnew);
	}
	dnew = rmx_alloc(rm->ncols, rm->nrows, rm->ncomp);
	if (!dnew)
		return(NULL);
	if (rm->info) {
		rmx_addinfo(dnew, rm->info);
		rmx_addinfo(dnew, "Transposed rows and columns\n");
	}
	dnew->dtype = rm->dtype;
	for (i = dnew->nrows; i--; )
	    for (j = dnew->ncols; j--; )
		for (k = dnew->ncomp; k--; )
			rmx_lval(dnew,i,j,k) = rmx_lval(rm,j,i,k);
	return(dnew);
}

/* Multiply (concatenate) two matrices and allocate the result */
RMATRIX *
rmx_multiply(const RMATRIX *m1, const RMATRIX *m2)
{
	RMATRIX	*mres;
	int	i, j, k, h;

	if (!m1 | !m2 || (m1->ncomp != m2->ncomp) | (m1->ncols != m2->nrows))
		return(NULL);
	mres = rmx_alloc(m1->nrows, m2->ncols, m1->ncomp);
	if (!mres)
		return(NULL);
	i = rmx_newtype(m1->dtype, m2->dtype);
	if (i)
		mres->dtype = i;
	else
		rmx_addinfo(mres, rmx_mismatch_warn);
	for (i = mres->nrows; i--; )
	    for (j = mres->ncols; j--; )
	        for (k = mres->ncomp; k--; ) {
		    long double	d = 0;
		    for (h = m1->ncols; h--; )
			d += rmx_lval(m1,i,h,k) * rmx_lval(m2,h,j,k);
		    rmx_lval(mres,i,j,k) = (double)d;
		}
	return(mres);
}

/* Element-wise multiplication (or division) of m2 into m1 */
int
rmx_elemult(RMATRIX *m1, const RMATRIX *m2, int divide)
{
	int	zeroDivides = 0;
	int	i, j, k;

	if (!m1 | !m2 || (m1->ncols != m2->ncols) | (m1->nrows != m2->nrows))
		return(0);
	if ((m2->ncomp > 1) & (m2->ncomp != m1->ncomp))
		return(0);
	i = rmx_newtype(m1->dtype, m2->dtype);
	if (i)
		m1->dtype = i;
	else
		rmx_addinfo(m1, rmx_mismatch_warn);
	for (i = m1->nrows; i--; )
	    for (j = m1->ncols; j--; )
		if (divide) {
		    double	d;
		    if (m2->ncomp == 1) {
			d = rmx_lval(m2,i,j,0);
			if (d == 0) {
			    ++zeroDivides;
			    for (k = m1->ncomp; k--; )
				rmx_lval(m1,i,j,k) = 0;
			} else {
			    d = 1./d;
			    for (k = m1->ncomp; k--; )
				rmx_lval(m1,i,j,k) *= d;
			}
		    } else
		        for (k = m1->ncomp; k--; ) {
			    d = rmx_lval(m2,i,j,k);
			    if (d == 0) {
			        ++zeroDivides;
				rmx_lval(m1,i,j,k) = 0;
			    } else
				rmx_lval(m1,i,j,k) /= d;
			}
		} else {
		    if (m2->ncomp == 1) {
			const double	d = rmx_lval(m2,i,j,0);
		        for (k = m1->ncomp; k--; )
			    rmx_lval(m1,i,j,k) *= d;
		    } else
		        for (k = m1->ncomp; k--; )
			    rmx_lval(m1,i,j,k) *= rmx_lval(m2,i,j,k);
		}
	if (zeroDivides) {
		rmx_addinfo(m1, "WARNING: zero divide(s) corrupted results\n");
		errno = ERANGE;
	}
	return(1);
}

/* Sum second matrix into first, applying scale factor beforehand */
int
rmx_sum(RMATRIX *msum, const RMATRIX *madd, const double sf[])
{
	double	*mysf = NULL;
	int	i, j, k;

	if (!msum | !madd ||
			(msum->nrows != madd->nrows) |
			(msum->ncols != madd->ncols) |
			(msum->ncomp != madd->ncomp))
		return(0);
	if (!sf) {
		mysf = (double *)malloc(sizeof(double)*msum->ncomp);
		if (!mysf)
			return(0);
		for (k = msum->ncomp; k--; )
			mysf[k] = 1;
		sf = mysf;
	}
	i = rmx_newtype(msum->dtype, madd->dtype);
	if (i)
		msum->dtype = i;
	else
		rmx_addinfo(msum, rmx_mismatch_warn);
	for (i = msum->nrows; i--; )
	    for (j = msum->ncols; j--; )
		for (k = msum->ncomp; k--; )
		     rmx_lval(msum,i,j,k) += sf[k] * rmx_lval(madd,i,j,k);
	if (mysf)
		free(mysf);
	return(1);
}

/* Scale the given matrix by the indicated scalar component vector */
int
rmx_scale(RMATRIX *rm, const double sf[])
{
	int	i, j, k;

	if (!rm | !sf)
		return(0);
	for (i = rm->nrows; i--; )
	    for (j = rm->ncols; j--; )
		for (k = rm->ncomp; k--; )
		    rmx_lval(rm,i,j,k) *= sf[k];

	if (rm->info)
		rmx_addinfo(rm, "Applied scalar\n");
	return(1);
}

/* Allocate new matrix and apply component transformation */
RMATRIX *
rmx_transform(const RMATRIX *msrc, int n, const double cmat[])
{
	int	i, j, ks, kd;
	RMATRIX	*dnew;

	if (!msrc | (n <= 0) | !cmat)
		return(NULL);
	dnew = rmx_alloc(msrc->nrows, msrc->ncols, n);
	if (!dnew)
		return(NULL);
	if (msrc->info) {
		char	buf[128];
		sprintf(buf, "Applied %dx%d component transform\n",
				dnew->ncomp, msrc->ncomp);
		rmx_addinfo(dnew, msrc->info);
		rmx_addinfo(dnew, buf);
	}
	dnew->dtype = msrc->dtype;
	for (i = dnew->nrows; i--; )
	    for (j = dnew->ncols; j--; )
	        for (kd = dnew->ncomp; kd--; ) {
		    double	d = 0;
		    for (ks = msrc->ncomp; ks--; )
		        d += cmat[kd*msrc->ncomp + ks] * rmx_lval(msrc,i,j,ks);
		    rmx_lval(dnew,i,j,kd) = d;
		}
	return(dnew);
}

/* Convert a color matrix to newly allocated RMATRIX buffer */
RMATRIX *
rmx_from_cmatrix(const CMATRIX *cm)
{
	int	i, j;
	RMATRIX	*dnew;

	if (!cm)
		return(NULL);
	dnew = rmx_alloc(cm->nrows, cm->ncols, 3);
	if (!dnew)
		return(NULL);
	dnew->dtype = DTfloat;
	for (i = dnew->nrows; i--; )
	    for (j = dnew->ncols; j--; ) {
		const COLORV	*cv = cm_lval(cm,i,j);
		rmx_lval(dnew,i,j,0) = cv[0];
		rmx_lval(dnew,i,j,1) = cv[1];
		rmx_lval(dnew,i,j,2) = cv[2];
	    }
	return(dnew);
}

/* Convert general matrix to newly allocated CMATRIX buffer */
CMATRIX *
cm_from_rmatrix(const RMATRIX *rm)
{
	int	i, j;
	CMATRIX	*cnew;

	if (!rm || rm->ncomp != 3)
		return(NULL);
	cnew = cm_alloc(rm->nrows, rm->ncols);
	if (!cnew)
		return(NULL);
	for (i = cnew->nrows; i--; )
	    for (j = cnew->ncols; j--; ) {
		COLORV	*cv = cm_lval(cnew,i,j);
		cv[0] = (COLORV)rmx_lval(rm,i,j,0);
		cv[1] = (COLORV)rmx_lval(rm,i,j,1);
		cv[2] = (COLORV)rmx_lval(rm,i,j,2);
	    }
	return(cnew);
}
