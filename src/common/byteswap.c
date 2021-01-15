#ifndef lint
static const char RCSid[] = "$Id: byteswap.c,v 3.2 2021/01/15 18:31:38 greg Exp $";
#endif
/*
 * Byte swapping routines
 *
 * External symbols declared in rtio.h
 */

#include "copyright.h"
#include "rtio.h"

void
swap16(		/* swap n 16-bit words */
	char  *wp,
	size_t  n
)
{
	int	t;

	while (n-- > 0) {
		t = wp[0]; wp[0] = wp[1]; wp[1] = t;
		wp += 2;
	}
}


void
swap32(		/* swap n 32-bit words */
	char  *wp,
	size_t  n
)
{
	int	t;

	while (n-- > 0) {
		t = wp[0]; wp[0] = wp[3]; wp[3] = t;
		t = wp[1]; wp[1] = wp[2]; wp[2] = t;
		wp += 4;
	}
}


void
swap64(		/* swap n 64-bit words */
	char  *wp,
	size_t  n
)
{
	int	t;

	while (n-- > 0) {
		t = wp[0]; wp[0] = wp[7]; wp[7] = t;
		t = wp[1]; wp[1] = wp[6]; wp[6] = t;
		t = wp[2]; wp[2] = wp[5]; wp[5] = t;
		t = wp[3]; wp[3] = wp[4]; wp[4] = t;
		wp += 8;
	}
}

