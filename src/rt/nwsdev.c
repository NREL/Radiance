#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

#ifndef lint
static char sccsid[]="@(#)two.c";
#endif
/*
 * NeWS driver
 *
 * July 1990
 */

#include <math.h>
#include "newsconstants.h"
#include "driver.h"
#include "nwsdev.h"
char inputbuffer[256];
int pos;
float gamma[257];

static int nws_close(),nws_clear(),nws_painter(),nws_getclick(),
           nws_printer(),nws_getinput(),nws_flush(),nws_errout();
static struct driver nws_driver =
 {
  nws_close,nws_clear,nws_painter,nws_getclick,
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
   gamma[i]=minfrac+(1-minfrac)*pow(i/256.,1./gammacorrection);
  ps_open_PostScript();
  getthebox(&wX,&wY,&wW,&wH);
  if(wW<100)wW=100;
  if(wH<100+textareaheight)wH=100+textareaheight;
  cps_initcanvas
   (wX,wY,wW,wH,(int)MB1,(int)MB2,(int)MB3);
  nws_driver.xsiz=wW;
  nws_driver.ysiz=wH-textareaheight;
  nws_driver.inpready=0;
  cmdvec=nws_printer;
  if(wrnvec!=NULL)wrnvec=nws_errout;
  return(&nws_driver);
 }

static int
nws_close()       /* close the display */
 {
  cmdvec=NULL;
  if(wrnvec!=NULL)wrnvec=stderr_v;
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
  stderr_v(msg);
  nws_printer(msg);
 }

static int
nws_painter(col,xmin,ymin,xmax,ymax)
float col[3];
int xmin,ymin,xmax,ymax;
 {
  int i;
  /* NeWS trashes the window if a float value less than 1/256 is sent
     to it.  Therefore, weed out all such values first */
  for(i=0;i<3;i++)
   {
    if(col[i]>1)col[i]=1;
    col[i]=gamma[(int)(col[i]*256)];
   }
  box(xmin,ymin+textareaheight,xmax,ymax+textareaheight
      ,col[0],col[1],col[2]);
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
