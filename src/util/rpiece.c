/* Copyright (c) 1992 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Generate sections of a picture.
 */
 
#include "standard.h"
#include <fcntl.h>
#include <signal.h>
#include "color.h"
#include "view.h"
#include "resolu.h"

				/* rpict command */
char  *rpargv[128] = {"rpict", "-S", "1", "-x", "512", "-y", "512", "-pa", "1"};
int  rpargc = 9;
int  rpd[3];
FILE  *torp, *fromrp;
COLR  *scanline;
				/* our view parameters */
VIEW  ourview = STDVIEW;
double  pixaspect = 1.0;
int  hres = 512, vres = 512, hmult = 2, vmult = 2;
				/* output file */
char  *outfile = NULL;
int  outfd;
long  scanorig;
int  syncfd = -1;		/* lock file descriptor */

char  *progname;
int  verbose = 0;

extern long  lseek(), ftell();

int  gotalrm = 0;
int  onalrm() { gotalrm++; }


main(argc, argv)
int  argc;
char  *argv[];
{
	register int  i, rval;
	
	progname = argv[0];
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-')
			switch (argv[i][1]) {
			case 'v':
				switch (argv[i][2]) {
				case '\0':	/* verbose option */
					verbose = !verbose;
					continue;
				case 'f':	/* view file */
					if (viewfile(argv[++i], &ourview, NULL) <= 0) {
						fprintf(stderr,
					"%s: not a view file\n", argv[i]);
						exit(1);
					}
					continue;
				default:	/* view option? */
					rval = getviewopt(&ourview, argc-i, argv+i);
					if (rval >= 0) {
						i += rval;
						continue;
					}
					break;
				}
				break;
			case 'p':		/* pixel aspect ratio? */
				if (argv[i][2] == 'a' && !argv[i][3])
					pixaspect = atof(argv[i+1]);
				break;
			case 'x':		/* piece x resolution */
				if (!argv[i][2])
					hres = atoi(argv[i+1]);
				break;
			case 'y':		/* piece y resolution */
				if (!argv[i][2])
					vres = atoi(argv[i+1]);
				break;
			case 'X':		/* horizontal multiplier */
				if (argv[i][2])
					break;
				hmult = atoi(argv[++i]);
				continue;
			case 'Y':		/* vertical multiplier */
				if (argv[i][2])
					break;
				vmult = atoi(argv[++i]);
				continue;
			case 'F':		/* syncronization file */
				if (argv[i][2])
					break;
				if ((syncfd = open(argv[++i], O_RDWR)) < 0) {
					fprintf(stderr, "%s: cannot open\n",
							argv[i]);
					exit(1);
				}
				continue;
			case 'o':		/* output file */
				if (argv[i][2])
					break;
				outfile = argv[++i];
				continue;
			}
		rpargv[rpargc++] = argv[i];
	}
	rpargv[rpargc] = NULL;
	if (outfile == NULL) {
		fprintf(stderr, "%s: missing output file\n", argv[0]);
		exit(1);
	}
	init(argc, argv);
	rpiece();
	rval = cleanup();
	exit(rval);
}


init(ac, av)			/* set up output file and start rpict */
int  ac;
char  **av;
{
	extern char  VersionID[];
	char  *err;
	FILE  *fp;
	int  hr, vr;
					/* set up view */
	if ((err = setview(&ourview)) != NULL) {
		fprintf(stderr, "%s: %s\n", progname, err);
		exit(1);
	}
	if (syncfd != -1) {
		char  buf[32];
		buf[read(syncfd, buf, sizeof(buf)-1)] = '\0';
		sscanf(buf, "%d %d", &hmult, &vmult);
	}
	normaspect(viewaspect(&ourview)*hmult/vmult, &pixaspect, &hres, &vres);
					/* open output file */
	if ((outfd = open(outfile, O_WRONLY|O_CREAT|O_EXCL, 0666)) >= 0) {
		if ((fp = fdopen(dup(outfd), "w")) == NULL)
			goto filerr;
		printargs(ac, av, fp);		/* write header */
		fprintf(fp, "SOFTWARE= %s\n", VersionID);
		fputs(VIEWSTR, fp);
		fprintview(&ourview, fp);
		putc('\n', fp);
		if (pixaspect < .99 || pixaspect > 1.01)
			fputaspect(pixaspect, fp);
		fputformat(COLRFMT, fp);
		putc('\n', fp);
		fprtresolu(hres*hmult, vres*vmult, fp);
	} else if ((outfd = open(outfile, O_RDWR)) >= 0) {
		if ((fp = fdopen(dup(outfd), "r+")) == NULL)
			goto filerr;
		getheader(fp, NULL);		/* skip header */
		if (fscnresolu(&hr, &vr, fp) < 0 ||	/* check resolution */
				hr != hres*hmult || vr != vres*vmult) {
			fprintf(stderr, "%s: picture resolution mismatch\n",
					outfile);
			exit(1);
		}
	} else {
		fprintf(stderr, "%s: cannot open\n", outfile);
		exit(1);
	}
	scanorig = ftell(fp);		/* record position of first scanline */
	if (fclose(fp) == -1)		/* done with stream i/o */
		goto filerr;
	sync();				/* avoid NFS buffering */
					/* start rpict process */
	if (open_process(rpd, rpargv) <= 0) {
		fprintf(stderr, "%s: cannot start %s\n", progname, rpargv[0]);
		exit(1);
	}
	if ((fromrp = fdopen(rpd[0], "r")) == NULL ||
			(torp = fdopen(rpd[1], "w")) == NULL) {
		fprintf(stderr, "%s: cannot open stream to %s\n",
				progname, rpargv[0]);
		exit(1);
	}
	if ((scanline = (COLR *)malloc(hres*sizeof(COLR))) == NULL) {
		fprintf(stderr, "%s: out of memory\n", progname);
		exit(1);
	}
	signal(SIGALRM, onalrm);
	return;
filerr:
	fprintf(stderr, "%s: file i/o error\n", outfile);
	exit(1);
}


int
nextpiece(xp, yp)		/* get next piece assignment */
int  *xp, *yp;
{
	extern char  *fgets();
	struct flock  fls;
	char  buf[64];

	if (gotalrm)			/* someone wants us to quit */
		return(0);
	if (syncfd != -1) {		/* use sync file */
		fls.l_type = F_WRLCK;		/* gain exclusive access */
		fls.l_whence = 0;
		fls.l_start = 0L;
		fls.l_len = 0L;
		fcntl(syncfd, F_SETLKW, &fls);
		lseek(syncfd, 0L, 0);
		buf[read(syncfd, buf, sizeof(buf)-1)] = '\0';
		if (sscanf(buf, "%*d %*d %d %d", xp, yp) < 2) {
			*xp = hmult;
			*yp = vmult-1;
		}
		if (--(*xp) < 0) {		/* decrement position */
			*xp = hmult-1;
			if (--(*yp) < 0) {	/* all done! */
				close(syncfd);
				return(0);
			}
		}
		sprintf(buf, "%d %d\n%d %d\n", hmult, vmult, *xp, *yp);
		lseek(syncfd, 0L, 0);		/* write new position */
		write(syncfd, buf, strlen(buf));
		fls.l_type = F_UNLCK;		/* release sync file */
		fcntl(syncfd, F_SETLKW, &fls);
		return(1);
	}
	if (fgets(buf, sizeof(buf), stdin) == NULL)	/* use stdin */
		return(0);
	if (sscanf(buf, "%d %d", xp, yp) == 2)
		return(1);
	fprintf(stderr, "%s: input format error\n", progname);
	exit(1);
}


int
cleanup()			/* close rpict process and clean up */
{
	register int  rpstat;

	free((char *)scanline);
	fclose(torp);
	fclose(fromrp);
	rpstat = close_process(rpd);
	if (rpstat == -1)
		rpstat = 0;
	return(rpstat);
}


rpiece()			/* render picture piece by piece */
{
	VIEW  pview;
	int  xorg, yorg;
	
	while (nextpiece(&xorg, &yorg)) {
		copystruct(&pview, &ourview);	/* compute view for piece */
		switch (ourview.type) {
		case VT_PER:
			pview.horiz = 2.*180./PI*atan(
					tan(PI/180./2.*ourview.horiz)/hmult );
			pview.vert = 2.*180./PI*atan(
					tan(PI/180./2.*ourview.vert)/vmult );
			break;
		case VT_PAR:
		case VT_ANG:
			pview.horiz = ourview.horiz / hmult;
			pview.vert = ourview.vert / vmult;
			break;
		case VT_HEM:
			pview.horiz = 2.*180./PI*asin(
					sin(PI/180./2.*ourview.horiz)/hmult );
			pview.vert = 2.*180./PI*asin(
					sin(PI/180./2.*ourview.vert)/vmult );
			break;
		default:
			fprintf(stderr, "%s: unknown view type '-vt%c'\n",
					progname, ourview.type);
			exit(1);
		}
		pview.hoff += xorg - 0.5*(hmult-1);
		pview.voff += yorg - 0.5*(vmult-1);
		fputs(VIEWSTR, torp);
		fprintview(&pview, torp);
		putc('\n', torp);
		fflush(torp);			/* assign piece to rpict */
		putpiece(xorg, yorg);		/* place piece in output */
		if (verbose) {			/* notify caller */
			printf("%d %d done\n", xorg, yorg);
			fflush(stdout);
		}
	}
}


putpiece(xpos, ypos)		/* get next piece from rpict */
int  xpos, ypos;
{
	int  hr, vr;
	int  y;

	if (xpos < 0 | ypos < 0 | xpos >= hmult | ypos >= vmult) {
		fprintf(stderr, "%s: requested piece (%d,%d) out of range\n",
				progname, xpos, ypos);
		exit(1);
	}
	getheader(fromrp, NULL);	/* discard header info. */
	if (fscnresolu(&hr, &vr, fromrp) < 0 ||	/* check resolution */
			hr != hres || vr != vres) {
		fprintf(stderr, "%s: resolution mismatch from %s\n",
				progname, rpargv[0]);
		exit(1);
	}
	for (y = 0; y < vr; y++) {	/* transfer scanlines */
		if (freadcolrs(scanline, hr, fromrp) < 0) {
			fprintf(stderr, "%s: read error from %s\n",
					progname, rpargv[0]);
			exit(1);
		}
		if ((y == 0 || hmult != 1) && lseek(outfd,
scanorig+((long)((vmult-1-ypos)*vres+y)*hmult*hres+xpos*hres)*sizeof(COLR),
				0) == -1) {
			fprintf(stderr, "%s: seek error\n", outfile);
			exit(1);
		}
		if (writebuf(outfd, (char *)scanline, hr*sizeof(COLR)) !=
				hr*sizeof(COLR)) {
			fprintf(stderr, "%s: write error\n", outfile);
			exit(1);
		}
	}
}
