/* RCSid: $Id$ */
/*
 *  random.h - header file for random(3) function.
 *
 *     10/1/85
 */

extern long  random();

#define  frandom()	(random()/2147483648.0)
