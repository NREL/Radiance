#ifndef lint
static char SCCSid[] = "$SunId$ AU";
#endif
/*
    rad2pict - Convert an Radiance image to APPLE pict format.
	
    School of Architecture, Auckland University, Private Bag
    Auckland, New Zealand
*/
#include <stdio.h>
#include <stdlib.h>
#include "pict.h"
#include "color.h"

char cbuf[8192*5]; 
char pbuf[8192]; 
int outbytes;
FILE *outf, *inf;
char **gargv;

putrect(xorg,yorg,xsize,ysize)
int xorg, yorg, xsize, ysize;
{
    putashort(yorg);
    putashort(xorg);
    putashort(ysize);
    putashort(xsize);
}

putfprect(xorg,yorg,xsize,ysize)
int xorg, yorg, xsize, ysize;
{
    putalong(yorg<<16);
    putalong(xorg<<16);
    putalong(ysize<<16);
    putalong(xsize<<16);
}

putalong(l)
long l;
{
    putbyte((l>>24)&0xff);
    putbyte((l>>16)&0xff);
    putbyte((l>>8)&0xff);
    putbyte((l>>0)&0xff);
}

putashort(s)
short s;
{
    putbyte((s>>8)&0xff);
    putbyte((s>>0)&0xff);
}

putbyte(b)
unsigned char b;
{
    char c[1];

    c[0] = b;
    if(!fwrite(c,1,1,outf)) {
	fprintf(stderr,"%s: error on write\n", gargv[0]);
	exit(1);
    }
    outbytes++;
}

putbytes(buf,n)
unsigned char *buf;
int n;
{
    if(!fwrite(buf,n,1,outf)) {
	fprintf(stderr,"topict: error on write\n");
	exit(1);
    }
    outbytes+=n;
}

main(argc,argv)
int argc;
char **argv;
{
    int xsize, ysize;
    int i, picsize;
    int ssizepos, lsizepos;

    gargv = argv;

    if( argc<3 ) {
	fprintf(stderr, "Usage: %s inimage out.pict\n", gargv[0]);
	exit(1);
    } 

    if( (inf=fopen(gargv[1],"rb")) == NULL ) {
	fprintf(stderr,"%s: can't open input file %s\n",gargv[0], gargv[1]);
	exit(1);
    }

    if( (outf=fopen(gargv[2],"wb")) == NULL ) {
	fprintf(stderr,"%s: can't open output file %s\n", gargv[0], gargv[1]);
	exit(1);
    }

    if (checkheader(inf, COLRFMT, NULL) < 0 ||
            fgetresolu(&xsize, &ysize, inf) != (YMAJOR|YDECR)) {
        fprintf(stderr, "%s: not a radiance picture\n", argv[1]);
        exit(1);
    }

    setcolrgam(2.0);

    for(i=0; i<HEADER_SIZE; i++) 
	putbyte(0);
    ssizepos = outbytes;
    putashort(0);	 	/* low 16 bits of file size less HEADER_SIZE */
    putrect(0,0,xsize,ysize);	/* bounding box of picture */
    putashort(PICT_picVersion);
    putashort(0x02ff);		/* version 2 pict */
    putashort(PICT_reservedHeader);	/* reserved header opcode */

    lsizepos = outbytes;
    putalong(0);  		/* full size of the file */
    putfprect(0,0,xsize,ysize);	/* fixed point bounding box of picture */
    putalong(0);  		/* reserved */

    putashort(PICT_clipRgn); 	/* the clip region */
    putashort(10);
    putrect(0,0,xsize,ysize);

    putpict(xsize, ysize);

    putashort(PICT_EndOfPicture); /* end of pict */

    picsize = outbytes-HEADER_SIZE;
    fseek(outf,ssizepos,0);
    putashort(picsize&0xffff);
    fseek(outf,lsizepos,0);
    putalong(picsize);

    fclose(outf);
    exit(0);
}

putpict(xsize, ysize)
int xsize;
int ysize;
{
    int y;
    int nbytes, rowbytes;

    putashort(PICT_Pack32BitsRect); /* 32 bit rgb */
    rowbytes = 4*xsize;
    putalong(0x000000ff);		/* base address */

    if(rowbytes&1)
	rowbytes++;
    putashort(rowbytes|0x8000);	/* rowbytes */
    putrect(0,0,xsize,ysize);	/* bounds */
    putashort(0);		/* version */

    putashort(4);	/* packtype */
    putalong(0);	/* packsize */
    putalong(72<<16);	/* hres */
    putalong(72<<16);	/* vres */

    putashort(16);	/* pixeltype */
    putashort(32);	/* pixelsize */
    putashort(3);	/* cmpcount */

    putashort(8);	/* cmpsize */
    putalong(0);	/* planebytes */
    putalong(0);	/* pmtable */
    putalong(0);	/* pmreserved */

    putrect(0,0,xsize,ysize);	/* scr rect */
    putrect(0,0,xsize,ysize);	/* dest rect */

    putashort(0x40);	/* transfer mode */
    for(y=0; y<ysize; y++) {
	getrow(inf, cbuf, xsize);

	nbytes = packbits(cbuf,pbuf,24*xsize);
	if(rowbytes>250) 
	    putashort(nbytes);
	else
	    putbyte(nbytes);
	putbytes(pbuf,nbytes);
    }

    if(outbytes&1) 
	putbyte(0);
}

int
getrow(in, mybuff, xsize)
FILE  *in;
char  *mybuff;
int  xsize;
{
    COLOR    color;
    COLR    *scanin = (COLR*) malloc(xsize * sizeof(COLR));
    int	    x;

    if (scanin == NULL) {
	printf("scanin null");
    }


    if (freadcolrs(scanin, xsize, in) < 0) {
        fprintf(stderr, " read error\n");
        exit(1);
    }

    colrs_gambs(scanin, xsize);
    
    for (x = 0; x < xsize; x++) {
	colr_color(color, scanin[x]);
	cbuf[xsize * 0 + x] = color[RED] * 255;
	cbuf[xsize * 1 + x] = color[GRN] * 255;
	cbuf[xsize * 2 + x] = color[BLU] * 255;
    }
    free(scanin);
}

packbits(ibits,pbits,nbits)
unsigned char *ibits, *pbits;
int nbits;
{
    int bytes;
    unsigned char *sptr;
    unsigned char *ibitsend;
    unsigned char *optr = pbits;
    int nbytes, todo, cc, count;

    nbytes = ((nbits-1)/8)+1;
    ibitsend = ibits+nbytes;
    while (ibits<ibitsend) {
	sptr = ibits;
	ibits += 2;
	while((ibits<ibitsend)&&((ibits[-2]!=ibits[-1])||(ibits[-1]!=ibits[0])))
	    ibits++;
 	if(ibits != ibitsend) {
	    ibits -= 2;
	}
	count = ibits-sptr;
	while(count) {
	    todo = count>127 ? 127:count;
	    count -= todo;
	    *optr++ = todo-1;
	    while(todo--)
		*optr++ = *sptr++;
	}
	if(ibits == ibitsend)
	    break;
	sptr = ibits;
	cc = *ibits++;
	while( (ibits<ibitsend) && (*ibits == cc) )
	    ibits++;
	count = ibits-sptr;
	while(count) {
	    todo = count>128 ? 128:count;
	    count -= todo;
	    *optr++ = 257-todo;
	    *optr++ = cc;
	}
    }
    return optr-pbits;
}
