/**
 *  Daysim module
 *
 * @author Augustinus Topor (topor@ise.fhg.de)
 */
#include <string.h>

#include "ray.h"
#include "standard.h"

#ifdef DAYSIM

/** default luminance of sky segment */
double daysimLuminousSkySegments = 1000.0;

/** default sort mode is modifier number */
int daysimSortMode = 1;

/** default number of specified sensors in zero */
int NumberOfSensorsInDaysimFile = 0;

/** default there are no sensor units assigned in rtrace_dc */
int *DaysimSensorUnits;

/** number of daylight coefficients */
static int daylightCoefficients = 0;

/*
 * Initializes the number of daylight coefficients
 */
int daysimInit( const int coefficients )
{
	daylightCoefficients = coefficients;

	return (daylightCoefficients >= 0) && (daylightCoefficients <= DAYSIM_MAX_COEFS);
}

/*
 * Returns the number of coefficients
 */
const int daysimGetCoefficients()
{
	return (const int)daylightCoefficients;
}

/*
 * Copies a daylight coefficient set
 */
void daysimCopy( DaysimCoef destin, const DaysimCoef source )
{
	memcpy(destin, source, daylightCoefficients * sizeof(DaysimNumber));
}

/*
 * Initializes all daylight coefficients with 'value'
 */
void daysimSet( DaysimCoef coef, const double value )
{
	int i;

	for( i = 0; i < daylightCoefficients; i++ )
		coef[i] = value;
}

/*
 * Scales the daylight coefficient set by the value 'scaling'
 */
void daysimScale( DaysimCoef coef, const double scaling )
{
	int i;

	for( i = 0; i < daylightCoefficients; i++ )
		coef[i] *= scaling;
}

/*
 * Adds two daylight coefficient sets:
 *	result[i] = result[i] + add[i]
 */
void daysimAdd( DaysimCoef result, const DaysimCoef add )
{
	int i;

	for( i = 0; i < daylightCoefficients; i++ )
		result[i] += add[i];
}

/*
 * Multiply two daylight coefficient sets:
 *	result[i] = result[i] * add[i]
 */
void daysimMult( DaysimCoef result, const DaysimCoef mult )
{
	int i;

	for( i = 0; i < daylightCoefficients; i++ )
		result[i] *= mult[i];
}

/*
 * Sets the daylight coefficient at position 'index' to 'value'
 */
void daysimSetCoef( DaysimCoef result, const int index, const double value )
{
	if (index < daylightCoefficients)
		result[index] = value;
}

/*
 * Adds 'value' to the daylight coefficient at position 'index'
 */
void daysimAddCoef( DaysimCoef result, const int index, const double add )
{
	if (index < daylightCoefficients)
		result[index] += add;
}

/*
 * Adds the elements of 'source' scaled by 'scaling'  to 'result'
 */
void daysimAddScaled( DaysimCoef result, const DaysimCoef add, const double scaling )
{
	int i;

	for( i = 0; i < daylightCoefficients; i++ )
		result[i] += add[i] * scaling;
}

/*
 * Assign the coefficients of 'source' scaled by 'scaling' to result
 */
void daysimAssignScaled( DaysimCoef result, const DaysimCoef source, const double scaling )
{
	int i;

	for( i = 0; i < daylightCoefficients; i++ )
		result[i] = source[i] * scaling;
}

/**
 * Check that the sum of daylight coefficients equals the red color channel
 */
void daysimCheck(DaysimCoef daylightCoef, const double value, const char* where)
{
	int    k;
	double ratio, sum;

	sum = 0.0;

	for (k = 0; k < daylightCoefficients; k++)
		sum += daylightCoef[k];

	if (sum >= value) { /* test whether the sum of daylight coefficients corresponds to value for red */
		if (sum == 0)
			ratio = 1.0;
		else
			ratio = value / sum;
	} else {
		if (value == 0)
			ratio = 1.0;
		else
			ratio = sum / value;
	}
	if (ratio < 0.9999) {
		sprintf(errmsg, "The sum of the daylight cofficients is %e and does not equal the total red illuminance %e at %s", sum, value, where);
		error(WARNING, errmsg);
	} else {
		//printf( "\n# ratio %12.9g\t[min( sum(DC)/ray-value, ray-value/sum(DC) )]", ratio );
	}
}

/*
 * Computes the sky/ground patch hit by a ray in direction (dx,dy,dz)
 * according to the Tregenza sky division.
 */
const DaysimSourcePatch daysimComputePatch( const FVECT dir )
{
	static DaysimSourcePatch number[8]= { 0, 30, 60, 84, 108, 126, 138, 144 };
	static double     ring_division[8]= { 30.0, 30.0, 24.0, 24.0, 18.0, 12.0, 6.0, 0.0 };
	int               ringnumber;
	DaysimSourcePatch patch;

	if( dir[2] > 0.0 ) {              // sky
		ringnumber=(int)(asin(dir[2])*15.0/PI);
		// origin of the number "15":
		// according to Tregenza, the celestial hemisphere is divided into 7 bands and
		// the zenith patch. The bands range from:
		//												altitude center
		// Band 1		0 to 12 Deg			30 patches	6
		// Band 2		12 to 24 Deg		30 patches	18
		// Band 3		24 to 36 Deg		24 patches	30
		// Band 4		36 to 48 Deg		24 patches	42
		// Band 5		48 to 60 Deg		18 patches	54
		// Band 6		60 to 72 Deg		12 patches	66
		// Band 7		72 to 84 Deg		 6 patches	78
		// Band 8		84 to 90 Deg		 1 patche	90
		// since the zenith patch is only takes 6Deg instead of 12, the arc length
		// between 0 and 90 Deg (equlas o and Pi/2) is divided into 7.5 units:
		// Therefore, 7.5 units = (int) asin(z=1)/(Pi/2)
		//				1 unit = asin(z)*(2*7.5)/Pi)
		//				1 unit = asin(z)*(15)/Pi)
		// Note that (int) always rounds to the next lowest integer
		if( dir[1] >= 0.0 )
			patch= number[ringnumber] + ring_division[ringnumber]*atan2(dir[1], dir[0])/(2.0*PI);
		else
			patch= number[ringnumber] + ring_division[ringnumber]*(atan2(dir[1], dir[0])/(2.0*PI) + 1.0);
	} else {                      // ground
		if( dir[2] >= -0.17365 ) {
			patch= 145;
		} else if( dir[2] >= -0.5 ) {
			patch= 146;
		} else {
			patch= 147;
		}
	}

	return patch;
}

//new version with a single ground DC
const DaysimSourcePatch daysimComputePatchDDS( const FVECT dir )
{
	static DaysimSourcePatch number[8]= { 0, 30, 60, 84, 108, 126, 138, 144 };
	static double     ring_division[8]= { 30.0, 30.0, 24.0, 24.0, 18.0, 12.0, 6.0, 0.0 };
	int               ringnumber;
	DaysimSourcePatch patch;

	if( dir[2] > 0.0 ) {              // sky
		ringnumber=(int)(asin(dir[2])*15.0/PI);
		// origin of the number "15":
		// according to Tregenza, the celestial hemisphere is divided into 7 bands and
		// the zenith path. The bands range from:
		//												altitude center
		// Band 1		0 to 12 Deg			30 patches	6
		// Band 2		12 to 24 Deg		30 patches	18
		// Band 3		24 to 36 Deg		24 patches	30
		// Band 4		36 to 48 Deg		24 patches	42
		// Band 5		48 to 60 Deg		18 patches	54
		// Band 6		60 to 72 Deg		12 patches	66
		// Band 7		72 to 84 Deg		 6 patches	78
		// Band 8		84 to 90 Deg		 1 patche	90
		// since the zenith patch is only takes 6Deg instead of 12, the arc length
		// between 0 and 90 Deg (equlas o and Pi/2) is divided into 7.5 units:
		// Therefore, 7.5 units = (int) asin(z=1)/(Pi/2)
		//				1 unit = asin(z)*(2*7.5)/Pi)
		//				1 unit = asin(z)*(15)/Pi)
		// Note that (int) always rounds to the next lowest integer
		if( dir[1] >= 0.0 )
			patch= number[ringnumber] + ring_division[ringnumber]*atan2(dir[1], dir[0])/(2.0*PI);
		else
			patch= number[ringnumber] + ring_division[ringnumber]*(atan2(dir[1], dir[0])/(2.0*PI) + 1.0);
	} else {                      // ground
		patch= 145;
	}

	return patch;
}

#endif
