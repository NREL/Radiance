/* RCSid $Id: bsdf.h,v 2.28 2021/03/27 17:50:18 greg Exp $ */
/*
 *  bsdf.h
 *  
 *  Declarations for bidirectional scattering distribution functions.
 *  Assumes <stdio.h> already included.
 *
 *  A material is oriented in right-hand coordinate system with X-axis
 *	in the surface plane pointed to the right as seen from the front.
 *	This means the Y-axis is "up" and the Z-axis is the surface normal.
 *
 *  Note that we reverse the identification of "front" and "back" from
 *	the conventions used in WINDOW 6.  "Front" in our library points
 *	in the +Z direction, towards the interior of the space rather
 *	than the exterior.
 *
 *  BSDF incident & exiting vectors are always oriented away from surface.
 *
 *  Created by Greg Ward on 1/10/11.
 *
 */

#ifndef _BSDF_H_
#define	_BSDF_H_

#include "fvect.h"
#include "ccolor.h"
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	SDnameLn	128		/* maximum BSDF name length */
#define	SDmaxCh		3		/* maximum # spectral channels */

/* Component flags for SDsampBSDF() and SDdirectHemi() */
#define	SDsampR		0x1		/* include reflection */
#define	SDsampT		0x2		/* include transmission */
#define	SDsampS		0x3		/* include scattering (R+T) */
#define	SDsampSp	0x4		/* include non-diffuse portion */
#define	SDsampDf	0x8		/* include diffuse portion */
#define	SDsampSpR	0x5		/* include non-diffuse reflection */
#define	SDsampSpT	0x6		/* include non-diffuse transmission */
#define	SDsampSpS	0x7		/* include non-diffuse scattering */
#define	SDsampAll	0xF		/* include everything */

/* Projected solid angle query flags for SDsizeBSDF() */
#define SDqueryVal	0x0		/* query single value */
#define	SDqueryMin	0x1		/* query minimum proj. solid angle */
#define	SDqueryMax	0x2		/* query maximum proj. solid angle */

/* Error codes: normal return, out of memory, file i/o, file format, bad argument,
		 bad data, unsupported feature, internal error, unknown error */
typedef enum {SDEnone=0, SDEmemory, SDEfile, SDEformat, SDEargument,
		SDEdata, SDEsupport, SDEinternal, SDEunknown} SDError;

/* English strings corresponding to ennumerated errors */
extern const char	*SDerrorEnglish[];

/* Pointer to error list in preferred language */
extern const char	**SDerrorList;

/* Additional information on last error (generally in English) */
extern char		SDerrorDetail[];

/* Holder for BSDF value and spectral color */
typedef struct {
	double		cieY;		/* photopic BSDF (Y) value */
	C_COLOR		spec;		/* spectral and (x,y) color */
} SDValue;

/* Cached, encoded, cumulative distribution for one incident (solid) angle */
#define SD_CDIST_BASE(styp)	double		cTotal;	\
				struct styp	*next
typedef struct SDCDst_s {
	SD_CDIST_BASE(SDCDst_s);	/* base fields first */
	/* ...encoded distribution extends struct */
} SDCDst;

extern const SDCDst	SDemptyCD;	/* empty distribution */

/* Forward declaration of BSDF component */
typedef struct SDComp_s	SDComponent;

/* Methods needed to handle BSDF components (nothing is optional) */
typedef struct {
					/* return non-diffuse BSDF */
	int		(*getBSDFs)(float coef[SDmaxCh], const FVECT outVec,
				    const FVECT inVec, SDComponent *sdc);
					/* query non-diffuse PSA for vector */
	SDError		(*queryProjSA)(double *psa, const FVECT v1,
						const RREAL *v2, int qflags,
						SDComponent *sdc);
					/* get cumulative distribution */
	const SDCDst	*(*getCDist)(const FVECT inVec, SDComponent *sdc);
					/* sample cumulative distribution */
	SDError		(*sampCDist)(FVECT ioVec, double randX,
						const SDCDst *cdp);
					/* free a spectral BSDF component */
	void		(*freeSC)(void *dist);
} SDFunc;

/* Structure to hold a spectral BSDF component (typedef SDComponent above) */
struct SDComp_s {
	C_COLOR		cspec[SDmaxCh];	/* component spectral bases */
	const SDFunc	*func;		/* methods for this component */
	void		*dist;		/* loaded distribution data */
	SDCDst		*cdList;	/* cumulative distribution cache */
};

/* Container for non-diffuse BSDF components */
typedef struct {
	double		minProjSA;	/* minimum projected solid angle */
	double		maxHemi;	/* maximum directional hemispherical */
	int		ncomp;		/* number of separate components */
	SDComponent	comp[1];	/* BSDF components (extends struct) */
} SDSpectralDF;

/* Loaded BSDF data */
typedef struct {
	char		name[SDnameLn];	/* BSDF name (usu. derived from file) */
	char		matn[SDnameLn];	/* material name */
	char		makr[SDnameLn];	/* manufacturer */
	char		*mgf;		/* geometric description (if any) */
	double		dim[3];		/* width, height, thickness (meters) */
	SDValue		rLambFront;	/* diffuse front reflectance */
	SDValue		rLambBack;	/* diffuse rear reflectance */
	SDValue		tLambFront;	/* diffuse front transmittance */
	SDValue		tLambBack;	/* diffuse back transmittance */
	SDSpectralDF	*rf, *rb;	/* non-diffuse BRDF components */
	SDSpectralDF	*tf, *tb;	/* non-diffuse BTDF components */
} SDData;

/* List of loaded BSDFs */
extern struct SDCache_s {
	SDData		bsdf;		/* BSDF data */
	unsigned	refcnt;		/* how many callers are using us? */
	struct SDCache_s		/* next in cache list */
			*next;
} *SDcacheList;		/* Global BSDF cache */

/* BSDF cache retention policies */
#define SDretainNone	0		/* free unreferenced BSDF data */
#define	SDretainBSDFs	1		/* keep loaded BSDFs in memory */
#define SDretainAll	2		/* also keep cumulative cache data */

extern int		SDretainSet;	/* =SDretainNone by default */
extern unsigned long	SDmaxCache;	/* =0 (unlimited) by default */

/*****************************************************************
 * The following routines are less commonly used by applications.
 */

#define	SDisLoaded(sd)	((sd)->rLambFront.spec.flags != 0)

/* Report an error to the indicated stream */
extern SDError		SDreportError(SDError ec, FILE *fp);

/* Shorten file path to useable BSDF name, removing suffix */
extern void		SDclipName(char res[SDnameLn], const char *fname);

/* Allocate new spectral distribution function */
extern SDSpectralDF	*SDnewSpectralDF(int nc);

/* Add component(s) to spectral distribution function */
extern SDSpectralDF	*SDaddComponent(SDSpectralDF *odf, int nadd);

/* Free a spectral distribution function */
extern void		SDfreeSpectralDF(SDSpectralDF *df);

/* Initialize an unused BSDF struct and assign name (calls SDclipName) */
extern void		SDclearBSDF(SDData *sd, const char *fname);

/* Load a BSDF struct from the given file (keeps name unchanged) */
extern SDError		SDloadFile(SDData *sd, const char *fname);

/* Free data associated with BSDF struct */
extern void		SDfreeBSDF(SDData *sd);

/* Find writeable BSDF by name, or allocate new cache entry if absent */
extern SDData		*SDgetCache(const char *bname);

/* Free cached cumulative distributions for BSDF component */
extern void		SDfreeCumulativeCache(SDSpectralDF *df);

/* Sample an individual BSDF component */
extern SDError		SDsampComponent(SDValue *sv, FVECT ioVec,
					double randX, SDComponent *sdc);

/* Convert 1-dimensional random variable to N-dimensional */
extern void		SDmultiSamp(double t[], int n, double randX);

/* Map a [0,1]^2 square to a unit radius disk */
extern void		SDsquare2disk(double ds[2], double seedx, double seedy);

/* Map point on unit disk to a unit square in [0,1]^2 range */
extern void		SDdisk2square(double sq[2], double diskx, double disky);

/*****************************************************************
 * The calls below are the ones most applications require.
 * All directions are assumed to be unit vectors.
 */

/* Get BSDF from cache (or load and cache it on first call) */
/* Report any problems to stderr, return NULL on failure */
extern const SDData	*SDcacheFile(const char *fname);

/* Free a BSDF from our cache (clear all if sd==NULL) */
extern void		SDfreeCache(const SDData *sd);

/* Query projected solid angle resolution for non-diffuse BSDF direction(s) */
extern SDError		SDsizeBSDF(double *projSA, const FVECT v1,
					const RREAL *v2, int qflags,
					const SDData *sd);

/* Return BSDF for the given incident and scattered ray vectors */
extern SDError		SDevalBSDF(SDValue *sv, const FVECT outVec,
					const FVECT inVec, const SDData *sd);

/* Compute directional hemispherical scattering at given incident angle */
extern double		SDdirectHemi(const FVECT inVec,
					int sflags, const SDData *sd);

/* Sample BSDF direction based on the given random variable */
extern SDError		SDsampBSDF(SDValue *sv, FVECT ioVec, double randX,
					int sflags, const SDData *sd);

/*****************************************************************
 * Vector math for getting between world and local BSDF coordinates.
 * Directions may be passed unnormalized to these routines.
 */

/* Compute World->BSDF transform from surface normal and BSDF up vector */
extern SDError		SDcompXform(RREAL vMtx[3][3], const FVECT sNrm,
					const FVECT uVec);

/* Compute inverse transform */
extern SDError		SDinvXform(RREAL iMtx[3][3], RREAL vMtx[3][3]);

/* Transform and normalize direction (column) vector */
extern SDError		SDmapDir(FVECT resVec, RREAL vMtx[3][3],
					const FVECT inpVec);

/* Application-specific BSDF loading routine (not part of our library) */
extern SDData		*loadBSDF(char *name);

/* Application-specific BSDF error translator (not part of our library) */
extern char		*transSDError(SDError ec);

#ifdef __cplusplus
}
#endif
#endif	/* ! _BSDF_H_ */
