/* Copyright (c) 1987 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 *  driver.h - header file for interactive device drivers.
 *
 *     2/2/87
 */

struct driver {				/* driver functions */
	int  (*close)();			/* close device */
	int  (*clear)();			/* clear device */
	int  (*paintr)();			/* paint rectangle */
	int  (*getcur)();			/* get cursor position */
	int  (*comout)();			/* command line output */
	int  (*comin)();			/* command line input */
	int  xsiz, ysiz;			/* device size */
	int  inpready;				/* input ready on device */
};

extern int  stderr_v();			/* error vectors */
extern int  (*wrnvec)(), (*errvec)(), (*cmdvec)();

extern struct driver  *comm_init();	/* stream interface */
					/* stream commands */
#define COM_CLEAR		0
#define COM_PAINTR		1
#define COM_GETCUR		2
#define COM_COMOUT		3
#define COM_COMIN		4
#define NREQUESTS		5	/* number of valid requests */

struct device {				/* interactive device */
	char  *name;				/* device name */
	char  *descrip;				/* description */
	struct driver  *(*init)();		/* initialize device */
};

extern struct device  devtable[];	/* supported devices */

#define  MB1		('\n')		/* mouse button 1 */
#define  MB2		('\r')		/* mouse button 2 */
#define  MB3		(' ')		/* mouse button 3 */
#define  ABORT		('C'-'@')	/* abort key */

#define  MAXRES		4000		/* preposterous display resolution */

/*
 *  struct driver *
 *  dname_init(name)
 *  char  *name;
 *  {
 *	Initialize device and return pointer to driver
 *	functions.  Returns NULL if an error occurred.
 *	The name string is used to identify the client.
 *	A device can be open by at most one client.
 *	Be verbose in error reports; call stderr_v().
 *	If device has its own error output, set errvec,
 *	cmdvec and wrnvec.
 *  }
 *  (*dev->close)()
 *  {
 *	Close the device.  Reset error vectors.
 *  }
 *  (*dev->clear)(xres, yres)
 *  int  xres, yres;
 *  {
 *	Clear the device for xres by yres output.  This call will
 *	be made prior to any output.
 *  }
 *  (*dev->paintr)(col, xmin, ymin, xmax, ymax)
 *  COLOR  col;
 *  int  xmin, ymin, xmax, ymax;
 *  {
 *	Paint a half-open rectangle from (xmin,ymin) to (xmax,ymax)
 *	with the color col.  Can call repaint() if necessary.
 *  }
 *  (*dev->getcur)(xp, yp)
 *  int  *xp, *yp;
 *  {
 *	Get the cursor position entered by the user via mouse,
 *	joystick, etc.  Return the key hit by the user (usually
 *	MB1 or MB2).  Return ABORT to cancel.
 *	Can be NULL for devices without this capability.
 *  }
 *  (*dev->comout)(out)
 *  char  *out;
 *  {
 *	Print the string out on the device command line.  If the
 *	string ends with '\n', the message is considered complete,
 *	and the next call can erase it.
 *  }
 *  (*dev->comin)(in)
 *  char  *in;
 *  {
 *	Read an edited input string from the command line.  If
 *	an unrecognized control character is entered, terminate
 *	input and return the string with only that character.
 *	The input string should not contain a newline.
 *	Must work in consort with comout.
 *  }
 *  xsiz, ysiz
 *	The maximum allowable x and y dimensions.  If any
 *	size is allowable, these should be set to MAXRES.
 *  inpready
 *	This variable should be made positive asynchronously
 *	when characters are ready on the input.  (Often easiest
 *	to check for input during calls to paintr.)
 */
