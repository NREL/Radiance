#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * NeWS driver, by Isaac Kwo
 *
 * July 1990
 */

#include "standard.h"
#include "newsconstants.h"
#include "driver.h"
#include "nwsdev.h"
#include "color.h"
char inputbuffer[256];
int pos;
int gamma[257];

static int nws_close(),nws_clear(),nws_painter(),nws_getclick(),
           nws_printer(),nws_getinput(),nws_flush(),nws_errout(),
           nws_gpainter();
static struct driver nws_driver =
 {
  nws_close,nws_clear,nws_gpainter,nws_getclick,
  nws_printer,nws_getinput,nws_flush,1.0
 };

static int
nws_clear()
 {
  cps_clear();
 }

static int
nws_getclick(xp,yp)
int *xp,*yp;
 {
  int key;
  cps_getclick(xp,yp,&key);
  nws_driver.inpready=0;
  return(key);
 }

struct driver *
nws_init(name,id)   /* initialize driver */
char *name,*id;
 {
  int wX,wY,wW,wH,i;
  gamma[256]=1;
  for(i=0;i<256;i++)
   gamma[i]=500*pow(i/256.,1./gammacorrection);
  ps_open_PostScript();
  sgicheck(&i);
  if(i)nws_driver.paintr=nws_painter;
  getthebox(&wX,&wY,&wW,&wH);
  if(wW<100)wW=100;
  if(wH<100+textareaheight)wH=100+textareaheight;
  cps_initcanvas
   (wX,wY,wW,wH,(int)MB1,(int)MB2,(int)MB3);
  nws_driver.xsiz=wW;
  nws_driver.ysiz=wH-textareaheight;
  nws_driver.inpready=0;
  erract[COMMAND].pf=nws_printer;
  if(erract[WARNING].pf!=NULL)erract[WARNING].pf=nws_errout;
  return(&nws_driver);
 }

static int
nws_close()       /* close the display */
 {
  erract[COMMAND].pf=NULL;
  if(erract[WARNING].pf!=NULL)erract[WARNING].pf=wputs;
  cps_cleanup();
  ps_flush_PostScript();
  ps_close_PostScript();
 }

static int
nws_flush()       /* flush output and check for keyboard input */
 {
  ps_flush_PostScript();
  isready(&(nws_driver.inpready));
 }

static int
nws_errout(msg)    /* output an error message */
char *msg;         /* my comments are so bogus */
 {
  eputs(msg);
  nws_printer(msg);
 }

static int
nws_painter(col,xmin,ymin,xmax,ymax)
COLOR col;
int xmin,ymin,xmax,ymax;
 {
  box(xmin,ymin+textareaheight,xmax,ymax+textareaheight
      ,(int)(500*col[RED]),(int)(500*col[GRN]),(int)(500*col[BLU]));
 }
static int
nws_gpainter(col,xmin,ymin,xmax,ymax)
COLOR col;
int xmin,ymin,xmax,ymax;
 {
  int i;
  int col2[3];
  for(i=0;i<3;i++)
   {
    col2[i]=256.*col[i];
    if(col2[i]>255)col2[i]=255;
    col2[i]=gamma[col2[i]];
   }
  box(xmin,ymin+textareaheight,xmax,ymax+textareaheight
      ,col2[0],col2[1],col2[2]);
 }
static int
nws_printer(orig) /* printer recognises \n as a linefeed */
char *orig;
 {
  char *m,*s,string[BUFSIZ]; /* s is for string and m is for message */
  m=s=string;
  while((*(s++))=(*(orig++)));
  s=string;
  while(*s)
   if(*s++=='\n')
    {
     *(s-1)=0;
     linefeed(m);
     m=s;
    }
  printout(m);
 }

static int
mygetc()
 {
  int key;
  getkey(&key);
  return(key);
 }
static int
myputs(str)
char *str;
 {
  char buf[2]; buf[1]=0;
  for(;*str;str++)
   switch(*str)
    {
     case '\n': pos=0; linefeed(""); break;
     case '\b':
      buf[0]=inputbuffer[--pos];
      delete(buf);
      break;
     default:
      buf[0]=inputbuffer[pos++]=(*str);
      printout(buf);
      break;
    }
  return(0);
 }
static int
nws_getinput(s,prompt)
char s[BUFSIZ];
 {
  if(prompt)nws_printer(prompt);
  startcomin();
  pos=0;
  editline(s,mygetc,myputs);
  endcomin();
  nws_driver.inpready=0;
 }
