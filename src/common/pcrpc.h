/* variables and declarations for client RPC library for software frame
   buffer approach */

#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/time.h>

/* if Limpel-Zev is used, a directive to define WIDEAREA is included
   on the compile line */


#define SUNFRONT	/* Sun is client */

#define PCPORT 2000	/* hard-wired PC port */

#define PROGNUM 1000	/* program number */
#define TESTVERS 1	/* version */

/* remote procedure numbers */

/* used in both metafile and software frame buffer approaches */

#define AUTHPROC 1	/* authorization procedure */
#define INITPROC 2	/* initialize VTR or videodisk */
#define RECORDPROC 4	/* record frame */
#define CLOSEPROC 5	/* relinquish control */

/* used only in software frame buffer approach */

#define NOCOMPRESS 6	/* no compression */
#define DISPLAY 7	/* display using TARGA graphics calls */
#define SENDUDP 13	/* send scan lines using UDP preliminary to
			   display */
#define MAPPROC 8	/* receive color map */

/* test RPC for rough timing estimates */

#define TESTRPC 14

/* definitions and variables common to both metafile and frame
   buffer approaches */

    /* give up after certain amount of time */
extern struct timeval total_timeout ;
    /* client handle */
extern CLIENT *client ;

#define UDP 0	/* UDP protocol */
#define TCP 1	/* TCP protocol */

extern short protocol ;	/* which protocol */

#define PREVIEW 0	/* preview mode */

extern FILE *vcr_rec ;	/* records starting and ending frame numbers */
extern int curr_frame ;	/* current frame number */
extern int copy_num ;	/* number of frames to record image on */
extern int record ;	/* preview or record */
extern bool_t optflag ;	/* video workstation has optical disk */

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

/* used only in software frame buffer approach */

#define NONE 0		/* no compression */
#define BTC 1		/* BTC compression */
#define COLORMAP 2	/* BTC and color map compression */
			/* hereafter referred to as color map */

extern short compression ;	/* type of compression */

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
    short linenum ;		/* scan line number (0 to 399) */
    int index ;			/* used by server as index into piece of
				   incoming buffer */
    int total1, total2 ;	/* server uses in building up compressed
				   frame buffer from incoming pieces */
#ifdef WIDEAREA
	/* number of bytes in compressed frame (Limpel_Zev only) */
    int total_lz ;
#endif
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
