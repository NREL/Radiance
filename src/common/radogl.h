/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 * Header file for Radiance - OpenGL routines.
 */

#include "standard.h"
#include <GL/glu.h>
#include "color.h"
#include "object.h"
#include "otypes.h"
#include "lookup.h"

#define MAXLIGHTS	8	/* number of OGL light sources */

#define MAXSPECEXP	128.	/* maximum allowed specular exponent */
#define UNKSPECEXP	25.	/* value to use when exponent unknown */

typedef struct {
	short	type;		/* material type (from otypes.h) */
	short	nlinks;		/* number of links to this material */
	union {
		struct {
			COLOR	ambdiff;	/* ambient and diffuse color */
			COLOR	specular;	/* specular color */
			GLfloat	specexp;	/* specular exponent */
		}	m;		/* regular material */
		struct {
			COLOR	emission;	/* emitting component */
			GLfloat	spotdir[3];	/* spot direction */
			GLfloat	spotang;	/* spot cutoff angle */
		}	l;		/* light source */
	}	u;		/* union of types */
}	MATREC;		/* OGL material properties */

extern double	expval;		/* global exposure value */
extern COLOR	ambval;		/* global ambient value */
extern int	glightid[MAXLIGHTS];	/* OpenGL GL_LIGHTi values */
extern int	dolights;	/* are we outputting light sources? */
extern int	domats;		/* are we doing materials? */

extern LUTAB	mtab;		/* material/modifier lookup table */

extern MATREC	*newmaterial(), *getmatp();

extern int	newglist();

extern double	checkoct();

#define issrcmat(m)	((m) != NULL && islight((m)->type) && \
				(m)->type != MAT_GLOW)
