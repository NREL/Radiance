/* Copyright (c) 1990 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 * Convert Neutral File Format input to Radiance scene description.
 *
 *	12/9/90		Greg Ward
 */

/******************************************************************

Since Eric Haines wrote such excellent documentation of his
Neutral File Format, I am just going to reprint it here with
my added comments in braces {}.

Neutral File Format (NFF), by Eric Haines

Draft document #1, 10/3/88

The NFF (Neutral File Format) is designed as a minimal scene description
language.  The language was designed in order to test various rendering
algorithms and efficiency schemes.  It is meant to describe the geometry and
basic surface characteristics of objects, the placement of lights, and the
viewing frustum for the eye.  Some additional information is provided for
esthetic reasons (such as the color of the objects, which is not strictly
necessary for testing rendering algorithms).

Future enhancements include:  circle and torus objects, spline surfaces
with trimming curves, directional lights, characteristics for positional
lights, CSG descriptions, and probably more by the time you read this.
Comments, suggestions, and criticisms are all welcome.

At present the NFF file format is used in conjunction with the SPD (Standard
Procedural Database) software, a package designed to create a variety of
databases for testing rendering schemes.  The SPD package is available
from Netlib and via ftp from drizzle.cs.uoregon.edu.  For more information
about SPD see "A Proposal for Standard Graphics Environments," IEEE Computer
Graphics and Applications, vol. 7, no. 11, November 1987, pp. 3-5.

By providing a minimal interface, NFF is meant to act as a simple format to
allow the programmer to quickly write filters to move from NFF to the
local file format.  Presently the following entities are supported:
     A simple perspective frustum
     A positional (vs. directional) light source description
     A background color description
     A surface properties description
     Polygon, polygonal patch, cylinder/cone, and sphere descriptions

Files are output as lines of text.  For each entity, the first line
defines its type.  The rest of the first line and possibly other lines
contain further information about the entity.  Entities include:

"v"  - viewing vectors and angles	{ optionally creates view file }
"l"  - positional light location	{ it's there, but bad to use }
"b"  - background color			{ ditto }
"f"  - object material properties	{ this is flakey }
"c"  - cone or cylinder primitive
"s"  - sphere primitive
"p"  - polygon primitive
"pp" - polygonal patch primitive	{ interpreted same as p for now }

These are explained in depth below:	{ see conversion routines }

***********************************************************************/

#include <stdio.h>

char	*viewfile = NULL;	/* view parameters file */

char	*progname;


main(argc, argv)		/* convert NFF file to Radiance */
int	argc;
char	*argv[];
{
	int	i;
	
	progname = argv[0];
	for (i = 1; i < argc; i++)
		if (argc-i > 1 && !strcmp(argv[i], "-vf"))
			viewfile = argv[++i];
		else
			break;
	if (i-argc > 1)
		goto userr;
	if (i-argc == 1 && freopen(argv[i], "r", stdin) == NULL) {
		perror(argv[i]);
		exit(1);
	}
	init();
	nff2rad();
	exit(0);
userr:
	fprintf(stderr, "Usage: %s [-vf viewfile] [input]\n", progname);
	exit(1);
}


init()			/* spit out initial definitions */
{
	printf("# File created by %s\n", progname);
	printf("\nvoid light light\n");
	printf("0\n0\n3 1 1 1\n");
	printf("\nvoid plastic fill\n");
	printf("0\n0\n5 .5 .5 .5 0 0\n");
}


nff2rad()		/* convert NFF on stdin to Radiance on stdout */
{
	register int	c;
	
	while ((c = getchar()) != EOF)
		switch (c) {
		case ' ':			/* white space */
		case '\t':
		case '\n':
		case '\f':
		case '\r':
			continue;
		case '#':			/* comment */
			comment();
			break;
		case 'v':			/* view point */
			view();
			break;
		case 'l':			/* light source */
			light();
			break;
		case 'b':			/* background color */
			background();
			break;
		case 'f':			/* fill material */
			fill();
			break;
		case 'c':			/* cylinder or cone */
			cone();
			break;
		case 's':			/* sphere */
			sphere();
			break;
		case 'p':			/* polygon or patch */
			poly();
			break;
		default:			/* unknown */
			fprintf(stderr, "%c: unknown NFF primitive\n", c);
			exit(1);
		}
}


/*******************************************

Comment.  Description:
    "#" [ string ]

Format:
    # [ string ]

    As soon as a "#" character is detected, the rest of the line is considered
    a comment.
    
******************/

comment()
{
	register int	c;
	
	putchar('#');
	while ((c = getchar()) != EOF) {
		putchar(c);
		if (c == '\n')
			break;
	}
}


/***************************************************

Viewpoint location.  Description:
    "v"
    "from" Fx Fy Fz
    "at" Ax Ay Az
    "up" Ux Uy Uz
    "angle" angle
    "hither" hither
    "resolution" xres yres

Format:

    v
    from %g %g %g
    at %g %g %g
    up %g %g %g
    angle %g
    hither %g
    resolution %d %d

The parameters are:

    From:  the eye location in XYZ.
    At:    a position to be at the center of the image, in XYZ world
	   coordinates.  A.k.a. "lookat".
    Up:    a vector defining which direction is up, as an XYZ vector.
    Angle: in degrees, defined as from the center of top pixel row to
	   bottom pixel row and left column to right column.
    Resolution: in pixels, in x and in y.

  Note that no assumptions are made about normalizing the data (e.g. the
  from-at distance does not have to be 1).  Also, vectors are not
  required to be perpendicular to each other.

  For all databases some viewing parameters are always the same:
    Yon is "at infinity."
    Aspect ratio is 1.0.

  A view entity must be defined before any objects are defined (this
  requirement is so that NFF files can be used by hidden surface machines).

***************/

view()
{
	static FILE	*fp = NULL;
	float	from[3], at[3], up[3], angle;
	
	if (scanf(" from %f %f %f", &from[0], &from[1], &from[2]) != 3)
		goto fmterr;
	if (scanf(" at %f %f %f", &at[0], &at[1], &at[2]) != 3)
		goto fmterr;
	if (scanf(" up %f %f %f", &up[0], &up[1], &up[2]) != 3)
		goto fmterr;
	if (scanf(" angle %f", &angle) != 1)
		goto fmterr;
	scanf(" hither %*f");
	scanf(" resolution %*d %*d");
	if (viewfile != NULL) {
		if (fp == NULL && (fp = fopen(viewfile, "a")) == NULL) {
			perror(viewfile);
			exit(1);
		}
		fprintf(fp,
	"VIEW= -vp %g %g %g -vd %g %g %g -vu %g %g %g -vh %g -vv %g\n",
				from[0], from[1], from[2],
				at[0]-from[0], at[1]-from[1], at[2]-from[2],
				up[0], up[1], up[2],
				angle, angle);
	}
	return;
fmterr:
	fprintf(stderr, "%s: view syntax error\n", progname);
	exit(1);
}


/********************************

Positional light.  A light is defined by XYZ position.  Description:
    "l" X Y Z

Format:
    l %g %g %g

    All light entities must be defined before any objects are defined (this
    requirement is so that NFF files can be used by hidden surface machines).
    Lights have a non-zero intensity of no particular value [this definition
    may change soon, with the addition of an intensity and/or color].

**************************/

light()
{
	static int	nlights = 0;
	register int	c;
	float	x, y, z;
	
	if (scanf("%f %f %f", &x, &y, &z) != 3) {
		fprintf(stderr, "%s: light source syntax error\n", progname);
		exit(1);
	}
	while ((c = getchar()) != EOF && c != '\n')
		;
	printf("\nlight sphere l%d ", ++nlights);
	printf("0\n0\n4 %g %g %g 1\n", x, y, z);
}


/**************************************************

Background color.  A color is simply RGB with values between 0 and 1:
    "b" R G B

Format:
    b %g %g %g

    If no background color is set, assume RGB = {0,0,0}.

********************/

background()
{
	float	r, g, b;
	
	if (scanf("%f %f %f", &r, &g, &b) != 3) {
		fprintf(stderr, "%s: background syntax error\n", progname);
		exit(1);
	}
	printf("\nvoid glow backg_color\n");
	printf("0\n0\n4 %g %g %g 0\n", r, g, b);
	printf("\nbackg_color source background\n");
	printf("0\n0\n4 0 0 1 360\n");
}


/****************************************************

Fill color and shading parameters.  Description:
     "f" red green blue Kd Ks Shine T index_of_refraction

Format:
    f %g %g %g %g %g %g %g %g

    RGB is in terms of 0.0 to 1.0.

    Kd is the diffuse component, Ks the specular, Shine is the Phong cosine
    power for highlights, T is transmittance (fraction of light passed per
    unit).  Usually, 0 <= Kd <= 1 and 0 <= Ks <= 1, though it is not required
    that Kd + Ks == 1.  Note that transmitting objects ( T > 0 ) are considered
    to have two sides for algorithms that need these (normally objects have
    one side).
  
    The fill color is used to color the objects following it until a new color
    is assigned.

*********************/

fill()
{
	float	r, g, b, d, s, p, t, n;
	
	if (scanf("%f %f %f %f %f %f %f %f", &r, &g, &b,
			&d, &s, &p, &t, &n) != 8) {
		fprintf(stderr, "%s: fill material syntax error\n", progname);
		exit(1);
	}
	d /= 1.-s-t;
	r *= d;
	g *= d;
	b *= d;
	if (p > 1.)
		p = 1./p;
	if (t > .001) {		/* has transmission */
		printf("\nvoid trans fill\n");
		printf("0\n0\n7 %g %g %g %g %g %g 1\n", r, g, b, s, p, t);
	} else {		/* no transmission */
		printf("\nvoid plastic fill\n");
		printf("0\n0\n5 %g %g %g %g %g\n", r, g, b, s, p);
	}
}


/*****************************************************

Cylinder or cone.  A cylinder is defined as having a radius and an axis
    defined by two points, which also define the top and bottom edge of the
    cylinder.  A cone is defined similarly, the difference being that the apex
    and base radii are different.  The apex radius is defined as being smaller
    than the base radius.  Note that the surface exists without endcaps.  The
    cone or cylinder description:

    "c"
    base.x base.y base.z base_radius
    apex.x apex.y apex.z apex_radius

Format:
    c
    %g %g %g %g
    %g %g %g %g

    A negative value for both radii means that only the inside of the object is
    visible (objects are normally considered one sided, with the outside
    visible).  Note that the base and apex cannot be coincident for a cylinder
    or cone.

************************/

cone()
{
	static int	ncs = 0;
	int	invert;
	float	x0, y0, z0, x1, y1, z1, r0, r1;
	
	if (scanf("%f %f %f %f %f %f %f %f", &x0, &y0, &z0, &r0,
			&x1, &y1, &z1, &r1) != 8) {
		fprintf(stderr, "%s: cylinder or cone syntax error\n",
				progname);
		exit(1);
	}
	if (invert = r0 < 0.) {
		r0 = -r0;
		r1 = -r1;
	}
	if (r0-r1 < .001 && r1-r0 < .001) {	/* cylinder */
		printf("\nfill %s c%d ", invert?"tube":"cylinder", ++ncs);
		printf("0\n0\n7\n");
		printf("\t%g\t%g\t%g\n", x0, y0, z0);
		printf("\t%g\t%g\t%g\n", x1, y1, z1);
		printf("\t%g\n", r0);
	} else {				/* cone */
		printf("\nfill %s c%d ", invert?"cup":"cone", ++ncs);
		printf("0\n0\n8\n");
		printf("\t%g\t%g\t%g\n", x0, y0, z0);
		printf("\t%g\t%g\t%g\n", x1, y1, z1);
		printf("\t%g\t%g\n", r0, r1);
	}
}


/*****************************************

Sphere.  A sphere is defined by a radius and center position:
    "s" center.x center.y center.z radius

Format:
    s %g %g %g %g

    If the radius is negative, then only the sphere's inside is visible
    (objects are normally considered one sided, with the outside visible).

******************/

sphere()
{
	static int	nspheres = 0;
	float	x, y, z, r;
	
	if (scanf("%f %f %f %f", &x, &y, &z, &r) != 4) {
		fprintf(stderr, "%s: sphere syntax error\n", progname);
		exit(1);
	}
	if (r < 0.) {
		printf("\nfill bubble s%d ", ++nspheres);
		printf("0\n0\n4 %g %g %g %g\n", x, y, z, -r);
	} else {
		printf("\nfill sphere s%d ", ++nspheres);
		printf("0\n0\n4 %g %g %g %g\n", x, y, z, r);
	}
}


/*********************************************

Polygon.  A polygon is defined by a set of vertices.  With these databases,
    a polygon is defined to have all points coplanar.  A polygon has only
    one side, with the order of the vertices being counterclockwise as you
    face the polygon (right-handed coordinate system).  The first two edges
    must form a non-zero convex angle, so that the normal and side visibility
    can be determined.  Description:

    "p" total_vertices
    vert1.x vert1.y vert1.z
    [etc. for total_vertices vertices]

Format:
    p %d
    [ %g %g %g ] <-- for total_vertices vertices

--------

Polygonal patch.  A patch is defined by a set of vertices and their normals.
    With these databases, a patch is defined to have all points coplanar.
    A patch has only one side, with the order of the vertices being
    counterclockwise as you face the patch (right-handed coordinate system).
    The first two edges must form a non-zero convex angle, so that the normal
    and side visibility can be determined.  Description:

    "pp" total_vertices
    vert1.x vert1.y vert1.z norm1.x norm1.y norm1.z
    [etc. for total_vertices vertices]

Format:
    pp %d
    [ %g %g %g %g %g %g ] <-- for total_vertices vertices

*******************/

poly()
{
	static int	npolys = 0;
	int	ispatch;
	int	nverts;
	float	x, y, z;
	
	ispatch = getchar();
	if (ispatch != 'p') {
		ungetc(ispatch, stdin);
		ispatch = 0;
	}
	if (scanf("%d", &nverts) != 1)
		goto fmterr;
	printf("\nfill polygon p%d ", ++npolys);
	printf("0\n0\n%d\n", 3*nverts);
	while (nverts-- > 0) {
		if (scanf("%f %f %f", &x, &y, &z) != 3)
			goto fmterr;
		if (ispatch)
			scanf("%*f %*f %*f");
		printf("\t%g\t%g\t%g\n", x, y, z);
	}
	return;
fmterr:
	fprintf(stderr, "%s: polygon or patch syntax error\n", progname);
	exit(1);
}
