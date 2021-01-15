#ifndef lint
static const char RCSid[] = "$Id: rcollate.c,v 2.37 2021/01/15 17:22:23 greg Exp $";
#endif
/*
 * Utility to re-order records in a binary or ASCII data file (matrix)
 */

#include <stdlib.h>
#include <ctype.h>
#include "platform.h"
#include "rtio.h"
#include "resolu.h"
#if defined(_WIN32) || defined(_WIN64)
  #undef ftello
  #define	ftello	ftell
  #undef ssize_t
  #define ssize_t	size_t
#else
  #include <sys/mman.h>
#endif

#define MAXLEVELS	16	/* max RxC.. block pairs */

typedef struct {
	void	*mapped;	/* memory-mapped pointer */
	void	*base;		/* pointer to base memory */
	size_t	len;		/* allocated memory length */
} MEMLOAD;		/* file loaded/mapped into memory */

typedef struct {
	int	nw_rec;		/* number of words per record */
	ssize_t	nrecs;		/* number of records we found */
	char	*rec[1];	/* record array (extends struct) */
} RECINDEX;

int		warnings = 1;	/* report warnings? */

/* free loaded file */
static void
free_load(MEMLOAD *mp)
{
	if (mp == NULL || (mp->base == NULL) | (mp->len <= 0))
		return;
#ifdef MAP_FILE
	if (mp->mapped)
		munmap(mp->mapped, mp->len);
	else
#endif
		free(mp->base);
	mp->mapped = NULL;
	mp->base = NULL;
	mp->len = 0;
}

/* load memory from an input stream, starting from current position */
static int
load_stream(MEMLOAD *mp, FILE *fp)
{
	size_t	alloced = 0;
	char	buf[8192];
	size_t	nr;

	if (mp == NULL)
		return(-1);
	mp->mapped = NULL;
	mp->base = NULL;
	mp->len = 0;
	if (fp == NULL)
		return(-1);
	while ((nr = fread(buf, 1, sizeof(buf), fp)) > 0) {
		if (!alloced)
			mp->base = malloc(alloced = nr);
		else if (mp->len+nr > alloced)
			mp->base = realloc(mp->base,
				alloced = alloced*(2+(nr==sizeof(buf)))/2+nr);
		if (mp->base == NULL)
			return(-1);
		memcpy((char *)mp->base + mp->len, buf, nr);
		mp->len += nr;
	}
	if (ferror(fp)) {
		free_load(mp);
		return(-1);
	}
	if (alloced > mp->len*5/4)	/* don't waste too much space */
		mp->base = realloc(mp->base, mp->len);
	return(mp->len > 0);
}

/* load a file into memory */
static int
load_file(MEMLOAD *mp, FILE *fp)
{
	int	fd;
	off_t	skip, flen, fpos;

#if defined(_WIN32) || defined(_WIN64)
				/* too difficult to fix this */
	return load_stream(mp, fp);
#endif
	if (mp == NULL)
		return(-1);
	mp->mapped = NULL;
	mp->base = NULL;
	mp->len = 0;
	if (fp == NULL)
		return(-1);
	fd = fileno(fp);
	skip = ftello(fp);
	flen = lseek(fd, 0, SEEK_END);
	if (flen <= skip)
		return((int)(flen - skip));
	mp->len = (size_t)(flen - skip);
#ifdef MAP_FILE
	if (mp->len > 1L<<20) {		/* map file if > 1 MByte */
		mp->mapped = mmap(NULL, flen, PROT_READ, MAP_PRIVATE, fd, 0);
		if (mp->mapped != MAP_FAILED) {
			mp->base = (char *)mp->mapped + skip;
			return(1);	/* mmap() success */
		}
		mp->mapped = NULL;	/* else fall back to reading it in... */
	}
#endif
	if (lseek(fd, skip, SEEK_SET) != skip ||
			(mp->base = malloc(mp->len)) == NULL) {
		mp->len = 0;
		return(-1);
	}
	fpos = skip;
	while (fpos < flen) {		/* read() fails if n > 2 GBytes */
		ssize_t	nread = read(fd, (char *)mp->base+(fpos-skip),
				(flen-fpos < 1L<<24) ? flen-fpos : 1L<<24);
		if (nread <= 0) {
			free_load(mp);
			return(-1);
		}
		fpos += nread;
	}
	return(1);
}

/* free a record index */
#define free_records(rp)	free(rp)

/* compute record index */
static RECINDEX *
index_records(const MEMLOAD *mp, int nw_rec)
{
	int		nall = 0;
	RECINDEX	*rp;
	char		*cp, *mend;
	int		n;

	if (mp == NULL || (mp->base == NULL) | (mp->len <= 0))
		return(NULL);
	if (nw_rec <= 0)
		return(NULL);
	nall = 1000;
	rp = (RECINDEX *)malloc(sizeof(RECINDEX) + nall*sizeof(char *));
	if (rp == NULL)
		return(NULL);
	rp->nw_rec = nw_rec;
	rp->nrecs = 0;
	cp = (char *)mp->base;
	mend = cp + mp->len;
	for ( ; ; ) {				/* whitespace-separated words */
		while (cp < mend && !*cp | isspace(*cp))
			++cp;
		if (cp >= mend)
			break;
		if (rp->nrecs >= nall) {
			nall += nall>>1;	/* get more record space */
			rp = (RECINDEX *)realloc(rp,
					sizeof(RECINDEX) + nall*sizeof(char *));
			if (rp == NULL)
				return(NULL);
		}
		rp->rec[rp->nrecs++] = cp;	/* point to first non-white */
		n = rp->nw_rec;
		while (++cp < mend)		/* find end of record */
			if (!*cp | isspace(*cp)) {
				if (--n <= 0)
					break;	/* got requisite # words */
				do {		/* else find next word */
					if (*cp == '\n') {
						fprintf(stderr,
						"Unexpected EOL in record!\n");
						free_records(rp);
						return(NULL);
					}
					if (++cp >= mend)
						break;
				} while (!*cp | isspace(*cp));
			}
	}
	rp->rec[rp->nrecs] = mend;		/* reallocate to save space */
	rp = (RECINDEX *)realloc(rp,
			sizeof(RECINDEX) + rp->nrecs*sizeof(char *));
	return(rp);
}

/* count number of columns based on first EOL */
static int
count_columns(const RECINDEX *rp)
{
	char	*cp = rp->rec[0];
	char	*mend = rp->rec[rp->nrecs];
	int	i;

	while (*cp != '\n')
		if (++cp >= mend)
			return(0);
	for (i = 0; i < rp->nrecs; i++)
		if (rp->rec[i] >= cp)
			break;
	return(i);
}

/* copy nth record from index to stdout */
static int
print_record(const RECINDEX *rp, ssize_t n)
{
	int	words2go = rp->nw_rec;
	char	*scp;

	if ((n < 0) | (n >= rp->nrecs))
		return(0);
	scp = rp->rec[n];
	do {
		putc(*scp++, stdout);
		if (!*scp | isspace(*scp)) {
			if (--words2go <= 0)
				break;
			putc(' ', stdout);	/* single space btwn. words */
			do
				if (++scp >= rp->rec[n+1])
					break;
			while (!*scp | isspace(*scp));
		}
	} while (scp < rp->rec[n+1]);
						/* caller adds record sep. */
	return(1);
}

/* copy a stream to stdout */
static int
output_stream(FILE *fp)
{
	char	buf[8192];
	ssize_t	n;

	if (fp == NULL)
		return(0);
	fflush(stdout);
	while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
		if (write(fileno(stdout), buf, n) != n)
			return(0);
	return(!ferror(fp));
}

/* get next word from stream, leaving stream on EOL or start of next word */
static char *
fget_word(char buf[256], FILE *fp)
{
	int	c;
	char	*cp;
					/* skip nul's and white space */
	while (!(c = getc(fp)) || isspace(c))
		;
	if (c == EOF)
		return(NULL);
	cp = buf;
	do
		*cp++ = c;
	while ((c = getc(fp)) != EOF && !isspace(c) && cp < buf+255);
	*cp = '\0';
	while (isspace(c) & (c != '\n'))
		c = getc(fp);
	if (c != EOF)
		ungetc(c, fp);
	return(buf);
}

char		*fmtid = NULL;			/* format id */
int		comp_size = 0;			/* binary bytes/channel */
int		n_comp = 0;			/* components/record */
int		ni_columns = 0;			/* number of input columns */
int		ni_rows = 0;			/* number of input rows */
int		no_columns = 0;			/* number of output columns */
int		no_rows = 0;			/* number of output rows */
int		transpose = 0;			/* transpose rows & cols? */
int		i_header = 1;			/* input header? */
int		o_header = 1;			/* output header? */
int		outArray[MAXLEVELS][2];		/* output block nesting */
int		outLevels = 0;			/* number of blocking levels */
int		check = 0;			/* force data check? */

/* parse RxCx... string */
static int
get_array(const char *spec, int blklvl[][2], int nlvls)
{
	int	n;

	if (nlvls <= 0) {
		fputs("Too many block levels!\n", stderr);
		exit(1);
	}
	if (sscanf(spec, "%dx%d", &blklvl[0][0], &blklvl[0][1]) != 2) {
		fputs("Bad block specification!\n", stderr);
		exit(1);
	}
	while (isdigit(*spec))
		spec++;
	spec++;		/* 'x' */
	while (isdigit(*spec))
		spec++;
	if ((*spec != 'x') & (*spec != 'X')) {
		if (*spec) {
			fputs("Blocks must be separated by 'x' or 'X'\n", stderr);
			exit(1);
		}
		return(1);
	}
	spec++;
	n = get_array(spec, blklvl+1, nlvls-1);
	if (!n)
		return(0);
	blklvl[0][0] *= blklvl[1][0];
	blklvl[0][1] *= blklvl[1][1];
	return(n+1);
}

/* check settings and assign defaults */
static int
check_sizes()
{
	if (fmtid == NULL) {
		fmtid = "ascii";
	} else if (!comp_size) {
		if (!strcmp(fmtid, "float"))
			comp_size = sizeof(float);
		else if (!strcmp(fmtid, "double"))
			comp_size = sizeof(double);
		else if (!strcmp(fmtid, "byte"))
			comp_size = 1;
		else if (strcmp(fmtid, "ascii")) {
			fprintf(stderr, "Unsupported format: %s\n", fmtid);
			return(0);
		}
	}
	if (transpose && (no_rows <= 0) & (no_columns <= 0)) {
		if (ni_rows > 0) no_columns = ni_rows;
		if (ni_columns > 0) no_rows = ni_columns;
	} else if ((no_rows <= 0) & (no_columns > 0) &&
			!((ni_rows*ni_columns) % no_columns))
		no_rows = ni_rows*ni_columns/no_columns;
	if (n_comp <= 0)
		n_comp = 3;
	return(1);
}

/* call to compute block input position */
static ssize_t
get_block_pos(int r, int c, int blklvl[][2], int nlvls)
{
	ssize_t	n = 0;

	while (nlvls > 1) {
		int	sr = r/blklvl[1][0];
		int	sc = c/blklvl[1][1];
		r -= sr*blklvl[1][0];
		c -= sc*blklvl[1][1];
		n += sr*blklvl[1][0]*blklvl[0][1] + sc*blklvl[1][0]*blklvl[1][1];
		blklvl++;
		nlvls--;
	}
	n += r*blklvl[0][1] + c;
	return(n);
}

/* return input offset based on array ordering and transpose option */
static ssize_t
get_input_pos(int r, int c)
{
	ssize_t	n;

	if (outLevels > 1) {		/* block reordering */
		n = get_block_pos(r, c, outArray, outLevels);
		if (transpose) {
			r = n/ni_rows;
			c = n - r*ni_rows;
			n = (ssize_t)c*ni_columns + r;
		}
	} else if (transpose)		/* transpose only */
		n = (ssize_t)c*ni_columns + r;
	else				/* XXX should never happen! */
		n = (ssize_t)r*no_columns + c;
	return(n);
}

/* output reordered ASCII or binary data from memory */
static int
do_reorder(const MEMLOAD *mp)
{
	static const char	tabEOL[2] = {'\t','\n'};
	RECINDEX		*rp = NULL;
	ssize_t			nrecords;
	int			i, j;
						/* propogate sizes */
	if (ni_rows <= 0)
		ni_rows = transpose ? no_columns : no_rows;
	if (ni_columns <= 0)
		ni_columns = transpose ? no_rows : no_columns;
						/* get # records (& index) */
	if (!comp_size) {
		if ((rp = index_records(mp, n_comp)) == NULL)
			return(0);
		if (ni_columns <= 0)
			ni_columns = count_columns(rp);
		nrecords = rp->nrecs;
	} else if ((ni_rows > 0) & (ni_columns > 0)) {
		nrecords = (ssize_t)ni_rows*ni_columns;
		if (nrecords > mp->len/(n_comp*comp_size)) {
			fputs("Input too small for specified size and type\n",
					stderr);
			return(0);
		}
	} else
		nrecords = mp->len/(n_comp*comp_size);
						/* check sizes */
	if ((ni_rows <= 0) & (ni_columns > 0))
		ni_rows = nrecords/ni_columns;
	else if ((ni_columns <= 0) & (ni_rows > 0))
		ni_columns = nrecords/ni_rows;
	if (nrecords != (ssize_t)ni_rows*ni_columns)
		goto badspec;
	if (transpose) {
		if (no_columns <= 0)
			no_columns = ni_rows;
		if (no_rows <= 0)
			no_rows = ni_columns;
		if (outLevels <= 1 &&
				(no_rows != ni_columns) | (no_columns != ni_rows))
			goto badspec;
	} else {
		if (no_columns <= 0)
			no_columns = ni_columns;
		if (no_rows <= 0)
			no_rows = ni_rows;
	}
	if (ni_rows*ni_columns != no_rows*no_columns) {
		fputs("Number of input and output records do not match\n",
				stderr);
		return(0);
	}
	if (o_header) {				/* finish header? */
		printf("NROWS=%d\n", no_rows);
		printf("NCOLS=%d\n", no_columns);
		fputc('\n', stdout);
	}
						/* reorder records */
	for (i = 0; i < no_rows; i++) {
	    for (j = 0; j < no_columns; j++) {
		ssize_t	n = get_input_pos(i, j);
		if (n >= nrecords) {
			fputs("Index past end-of-file\n", stderr);
			return(0);
		}
		if (rp != NULL) {		/* ASCII output */
			print_record(rp, n);
			putc(tabEOL[j >= no_columns-1], stdout);
		} else {			/* binary output */
			putbinary((char *)mp->base + (n_comp*comp_size)*n,
					comp_size, n_comp, stdout);
		}
	    }
	    if (ferror(stdout)) {
		fprintf(stderr, "Error writing to stdout\n");
		return(0);
	    }
	}
	if (rp != NULL)
		free_records(rp);
	return(1);
badspec:
	fprintf(stderr, "Bad dimension(s)\n");
	return(0);
}

/* resize ASCII stream input by ignoring EOLs between records */
static int
do_resize(FILE *fp)
{
	ssize_t	records2go = ni_rows*ni_columns;
	int	columns2go = no_columns;
	char	word[256];
			
	if (o_header) {				/* finish header? */
		if (no_rows > 0)
			printf("NROWS=%d\n", no_rows);
		if (no_columns > 0)
			printf("NCOLS=%d\n", no_columns);
		fputc('\n', stdout);
	}
						/* sanity checks */
	if (comp_size || !check &
			(no_columns == ni_columns) & (no_rows == ni_rows))
		return(output_stream(fp));	/* no-op -- just copy */
	if (no_columns <= 0) {
		fprintf(stderr, "Missing -oc specification\n");
		return(0);
	}
	if ((records2go <= 0) & (no_rows > 0))
		records2go = no_rows*no_columns;
	else if (no_rows*no_columns != records2go) {
		fprintf(stderr,
			"Number of input and output records disagree (%dx%d != %dx%d)\n",
				ni_rows, ni_columns, no_rows, no_columns);
		return(0);
	}
	do {					/* reshape records */
		int	n;

		for (n = n_comp; n--; ) {
			if (fget_word(word, fp) == NULL) {
				if (records2go > 0 || n < n_comp-1)
					break;
				goto done;	/* normal EOD */
			}
			fputs(word, stdout);
			if (n) {		/* mid-record? */
				int	c = getc(fp);
				if ((c == '\n') | (c == EOF))
					break;
				ungetc(c, fp);
				putc(' ', stdout);
			}
		}
		if (n >= 0) {
			fprintf(stderr, "Incomplete record / unexpected EOF\n");
			return(0);
		}
		if (--columns2go <= 0) {	/* time to end output row? */
			putc('\n', stdout);
			columns2go = no_columns;
		} else				/* else separate records */
			putc('\t', stdout);
	} while (--records2go);			/* expected EOD? */
done:
	if (warnings && columns2go != no_columns)
		fprintf(stderr, "Warning -- incomplete final row\n");
	if (warnings && fget_word(word, fp) != NULL)
		fprintf(stderr, "Warning -- characters beyond expected EOD\n");
	return(1);
}

/* process a header line and copy to stdout */
static int
headline(char *s, void *p)
{
	static char	fmt[MAXFMTLEN];
	int		n;

	if (formatval(fmt, s)) {
		if (fmtid == NULL) {
			fmtid = fmt;
			return(0);
		}
		if (!strcmp(fmt, fmtid))
			return(0);
		fprintf(stderr, "Input format '%s' != '%s'\n", fmt, fmtid);
		return(-1);
	}
	if (!strncmp(s, "NROWS=", 6)) {
		n = atoi(s+6);
		if ((ni_rows > 0) & (n != ni_rows)) {
			fputs("Incorrect input row count\n", stderr);
			return(-1);
		}
		ni_rows = n;
		return(0);
	}
	if (!strncmp(s, "NCOLS=", 6)) {
		n = atoi(s+6);
		if ((ni_columns > 0) & (n != ni_columns)) {
			fputs("Incorrect input column count\n", stderr);
			return(-1);
		}
		ni_columns = n;
		return(0);
	}
	if (!strncmp(s, "NCOMP=", 6)) {
		n = atoi(s+6);
		if ((n_comp > 0) & (n != n_comp)) {
			fputs("Incorrect number of components\n", stderr);
			return(-1);
		}
		n_comp = n;
		return(0);
	}
	if (o_header)
		fputs(s, stdout);		/* copy header info. */
	return(0);
}

/* main routine for converting rows/columns in data file */
int
main(int argc, char *argv[])
{
	int	a;

	for (a = 1; a < argc && argv[a][0] == '-'; a++)
		switch (argv[a][1]) {
		case 'i':			/* input */
			if (argv[a][2] == 'c')	/* columns */
				ni_columns = atoi(argv[++a]);
			else if (argv[a][2] == 'r')
				ni_rows = atoi(argv[++a]);
			else
				goto userr;
			break;
		case 'o':			/* output */
			if (argv[a][2] == 'c')	/* columns */
				no_columns = atoi(argv[++a]);
			else if (argv[a][2] == 'r')
				no_rows = atoi(argv[++a]);
			else if (argv[a][2] ||
			  !(outLevels=get_array(argv[++a], outArray, MAXLEVELS)))
				goto userr;
			break;
		case 'h':			/* turn off header */
			switch (argv[a][2]) {
			case 'i':
				i_header = 0;
				break;
			case 'o':
				o_header = 0;
				break;
			case '\0':
				i_header = o_header = 0;
				break;
			default:
				goto userr;
			}
			break;
		case 't':			/* transpose on/off */
			transpose = !transpose;
			break;
		case 'f':			/* format */
			switch (argv[a][2]) {
			case 'a':		/* ASCII */
			case 'A':
				fmtid = "ascii";
				comp_size = 0;
				break;
			case 'f':		/* float */
			case 'F':
				fmtid = "float";
				comp_size = sizeof(float);
				break;
			case 'd':		/* double */
			case 'D':
				fmtid = "double";
				comp_size = sizeof(double);
				break;
			case 'b':		/* binary (bytes) */
			case 'B':
				fmtid = "byte";
				comp_size = 1;
				break;
			default:
				goto userr;
			}
			if (argv[a][3]) {
				if (!isdigit(argv[a][3]))
					goto userr;
				n_comp = atoi(argv[a]+3);
			} else
				n_comp = 1;
			break;
		case 'w':			/* warnings on/off */
			warnings = !warnings;
			break;
		case 'c':			/* force check operation */
			check = 1;
			break;
		default:
			goto userr;
		}
	if (a < argc-1)				/* arg count OK? */
		goto userr;
	if (outLevels) {			/* should check consistency? */
		no_rows = outArray[0][0];
		no_columns = outArray[0][1];
	}
						/* open input file? */
	if (a == argc-1 && freopen(argv[a], "r", stdin) == NULL) {
		fprintf(stderr, "%s: cannot open for reading\n", argv[a]);
		return(1);
	}
	if (comp_size) {
		SET_FILE_BINARY(stdin);
		SET_FILE_BINARY(stdout);
	}
						/* check for no-op */
	if (!transpose & !check & (outLevels <= 1) & (i_header == o_header) &&
			(no_columns == ni_columns) & (no_rows == ni_rows)) {
		if (warnings)
			fprintf(stderr, "%s: no-op -- copying input verbatim\n",
				argv[0]);
		if (!output_stream(stdin))
			return(1);
		return(0);
	}
	if (i_header) {				/* read header */
		if (getheader(stdin, headline, NULL) < 0)
			return(1);
		if (!check_sizes())
			return(1);
		if (comp_size) {		/* a little late... */
			SET_FILE_BINARY(stdin);
			SET_FILE_BINARY(stdout);
		}
	} else if (!check_sizes())
		return(1);
	if (o_header) {				/* write/add to header */
		if (!i_header)
			newheader("RADIANCE", stdout);
		printargs(a, argv, stdout);
		printf("NCOMP=%d\n", n_comp);
		fputformat(fmtid, stdout);
	}
	if (transpose | check | (outLevels > 1) || (o_header && no_rows <= 0)) {
		MEMLOAD	myMem;			/* need to map into memory */
		if (a == argc-1) {
			if (load_file(&myMem, stdin) <= 0) {
				fprintf(stderr, "%s: error loading file into memory\n",
						argv[a]);
				return(1);
			}
		} else if (load_stream(&myMem, stdin) <= 0) {
			fprintf(stderr, "%s: error loading stdin into memory\n",
						argv[0]);
			return(1);
		}
		if (!do_reorder(&myMem))
			return(1);
		/* free_load(&myMem);	about to exit, so don't bother */
	} else if (!do_resize(stdin))		/* just reshaping input */
		return(1);
	return(0);
userr:
	fprintf(stderr,
"Usage: %s [-h[io]][-w][-c][-f[afdb][N]][-t][-ic in_col][-ir in_row][-oc out_col][-or out_row][-o RxC[xR1xC1..]] [input.dat]\n",
			argv[0]);
	return(1);
}
