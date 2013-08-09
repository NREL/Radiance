/* RCSid $Id$ */
/*
 * Header file for solar position calculations
 */

#ifndef _RAD_SUN_H_
#define _RAD_SUN_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int jdate(int month, int day);
extern double stadj(int  jd);
extern double sdec(int  jd);
extern double salt(double sd, double st);
extern double sazi(double sd, double st);

#ifdef __cplusplus
}
#endif

#endif /* _RAD_SUN_H_ */
