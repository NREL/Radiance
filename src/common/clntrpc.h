/* clntrpc.h      Distribution 1.0   88/9/19   Scry */

/*   The Scry system is copyright (C) 1988, Regents  of  the
University  of  California.   Anyone may reproduce ``Scry'',
the software in this distribution, in whole or in part, pro-
vided that:

(1)  Any copy  or  redistribution  of  Scry  must  show  the
     Regents  of  the  University of California, through its
     Lawrence Berkeley Laboratory, as the source,  and  must
     include this notice;

(2)  Any use of this software must reference this  distribu-
     tion,  state that the software copyright is held by the
     Regents of the University of California, and  that  the
     software is used by their permission.

     It is acknowledged that the U.S. Government has  rights
in  Scry  under  Contract DE-AC03-765F00098 between the U.S.
Department of Energy and the University of California.

     Scry is provided as a professional  academic  contribu-
tion  for  joint exchange.  Thus it is experimental, is pro-
vided ``as is'', with no warranties of any kind  whatsoever,
no  support,  promise  of updates, or printed documentation.
The Regents of the University of California  shall  have  no
liability  with respect to the infringement of copyrights by
Scry, or any part thereof.     */


#include <netdb.h>
#ifdef CRAY
#include <rpc/types.h>
#include "in.h"  /* YUCK */
#include <sys/socket.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpc/auth_unix.h>
#include <rpc/svc.h>
#include <rpc/svc_auth.h>
#else
#include <rpc/rpc.h>
#include <sys/socket.h>
#endif
#include <sys/time.h>

#define SUNFRONT	/* Sun is client */

#define PCPROGRAM 300000
#define PCPORT 2000
#define TESTVERS 1	/* version */

/* remote procedure numbers */

#define AUTHPROC 1	/* authorization procedure */
#define INITPROC 2	/* initialize VTR or videodisk */
#define RECORDPROC 3	/* record frame */
#define CLOSEPROC 4	/* relinquish control */
#define NOCOMPRESS 5	/* send uncompressed image data */
#define COLORSEND 6	/* send and display colormap-compressed scan lines */
#define SENDUDP 7	/* send BTC and colormap-compressed scan lines */
#define DISPLAY 8	/* display BTC and colormap-compressed scan lines */
#define MAPPROC 9	/* send color map */

    /* give up after certain amount of time */
extern struct timeval s_total_timeout ;
    /* client handle */
extern CLIENT *s_client ;

#define UDP 0	/* UDP protocol */
#define TCP 1	/* TCP protocol */

extern short s_protocol ;	/* which protocol */

#define PREVIEW 0	/* preview mode */

extern int s_curr_frame ;	/* current frame number */
extern int s_copy_num ;	/* number of frames to record image on */
extern int s_record ;	/* preview or record */
extern bool_t s_optflag ;	/* video workstation has optical disk */
extern bool_t s_lempel_ziv ;	/* use Lempel-Ziv compression or not */

struct netuser		/* user description structure */
{
    int nu_uid ;	/* effective user id */
    unsigned int nu_glen ;	/* number of groups user in */
    int *nu_gids ;	/* groups user is in */
} ;

struct record
{
    int in_at ;		/* start recording image at */
    int out_at ;	/* finish recording image at */
} ;

struct recinfo
{
    bool_t optflag ;
    int start_frame ;
    int total ;
} ;

#define NONE 0		/* no compression */
#define BTC 1		/* BTC compression */
#define COLORMAP 2	/* color map compression */
#define LEMPEL_ZIV 4	/* Lempel-Ziv compression */
#define FRAME_FRAME 8	/* frame-to-frame differencing */

extern short s_compression ;	/* type of compression */

struct mapstore
{
    short mapnum ;		/* number of entries in color map */
    unsigned short map[256] ;	/* entries in color map */
} ;

    
struct scan_packet
{
    bool_t first ;		/* first frame - no frame-frame differencing */
    int record ;		/* preview or record */
    short compression ;		/* type of compression */
    int lempel_ziv ;		/* Lempel-Ziv used or not */
    short linenum ;		/* scan line number (0 to 399) */
    int index ;			/* used by server as index into piece of
				   incoming buffer */
    int total1, total2 ;	/* server uses in building up compressed
				   frame buffer from incoming pieces */
	/* number of bytes in compressed frame (Lempel-Ziv only) */
    int total_lz ;
    int in_at, out_at ;		/* starting, ending numbers to
				   record image on */
    short mapnum ;		/* number of entries in color map */
    unsigned short map[256] ;	/* color map */
    short num_bytes[100] ;	/* number of bytes in each group of
				   4 compressed scan lines */
    unsigned char pack[76800] ;	/* compressed frame buffer or piece thereof */
				/* 2 scan lines stored here when compression
				   not used */
} ;
