/**
 *  Daysim header file
 *
 * @author Augustinus Topor (topor@ise.fhg.de)
 */

#ifndef DAYSIM_H
#define DAYSIM_H

#ifdef DAYSIM

#include "fvect.h"



#define DAYSIM_MAX_COEFS		148
//#define DAYSIM_MAX_COEFS		2306

/**
 * daylight coefficient
 */
typedef float DaysimNumber;

typedef DaysimNumber DaysimCoef[DAYSIM_MAX_COEFS];

/* Index to sky source patch */
typedef unsigned char DaysimSourcePatch;


/** */
extern double daysimLuminousSkySegments;

/** */
extern int daysimSortMode;

/** */
extern int NumberOfSensorsInDaysimFile;

/** */
extern int *DaysimSensorUnits;

/** Initializes the number of daylight coefficients */
extern int daysimInit(const int coefficients);

/** Returns the number of coefficients */
extern const int daysimGetCoefficients();

/** Copies a daylight coefficient set */
extern void daysimCopy(DaysimCoef destin, const DaysimCoef source);

/** Initialises all daylight coefficients with 'value' */
extern void daysimSet(DaysimCoef coef, const double value);

/** Scales the daylight coefficient set by the value 'scaling' */
extern void daysimScale(DaysimCoef coef, const double scaling);

/** Adds two daylight coefficient sets:
	result[i]= result[i] + add[i] */
extern void daysimAdd(DaysimCoef result, const DaysimCoef add);

/** Multiply two daylight coefficient sets:
	result[i]= result[i] * add[i] */
extern void daysimMult(DaysimCoef result, const DaysimCoef mult);

/** Sets the daylight coefficient at position 'index' to 'value' */
extern void daysimSetCoef(DaysimCoef result, const int index, const double value);

/** Adds 'value' to the daylight coefficient at position 'index' */
extern void daysimAddCoef(DaysimCoef result, const int index, const double add);

/** Adds the elements of 'source' scaled by 'scaling'  to 'result' */
extern void daysimAddScaled(DaysimCoef result, const DaysimCoef add, const double scaling);

/** Assign the coefficients of 'source' scaled by 'scaling' to result */
extern void daysimAssignScaled(DaysimCoef result, const DaysimCoef source, const double scaling);

/** Check that the sum of daylight coefficients equals the red color channel */
extern void daysimCheck(DaysimCoef daylightCoef, const double value, const char* where);

/* Computes the sky/ground patch hit by a ray in direction (dx,dy,dz)
 * according to the Tregenza sky division. */
extern const DaysimSourcePatch daysimComputePatch(const FVECT dir);
#endif /* DAYSIM */


#endif /* DAYSIM_H */
