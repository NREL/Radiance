#ifndef lint
static const char RCSid[] = "$Id: pmapdump.c,v 2.13 2019/01/10 17:32:39 rschregle Exp $";
#endif

/* 
   ======================================================================
   Dump photon maps as RADIANCE scene description to stdout

   Roland Schregle (roland.schregle@{hslu.ch, gmail.com})
   (c) Fraunhofer Institute for Solar Energy Systems,
   (c) Lucerne University of Applied Sciences and Arts,
       supported by the Swiss National Science Foundation (SNSF, #147053)
   ======================================================================
   
   $Id: pmapdump.c,v 2.13 2019/01/10 17:32:39 rschregle Exp $
*/



#include "pmapio.h"
#include "pmapparm.h"
#include "pmaptype.h"
#include "rtio.h"
#include "resolu.h"
#include "random.h"
#include "math.h"


/* Defaults */
/*    Sphere radius as fraction of avg. intersphere dist */
/*    Relative scale for sphere radius (fudge factor) */
/*    Number of spheres */
#define RADCOEFF 0.05
#define RADSCALE 1.0
#define NSPHERES 10000


/* RADIANCE material and object defs for each photon type */
typedef struct {
   char *mat, *obj;
} RadianceDef;


/* We use %e for the material def to preserve precision when outputting
   photon flux */
const RadianceDef radDefs [] = {
   {  "void glow mat.global\n0\n0\n4 %e %e %e 0\n",
      "mat.global sphere obj.global\n0\n0\n4 %g %g %g %g\n"
   },
   {  "void glow mat.pglobal\n0\n0\n4 %e %e %e 0\n",
      "mat.pglobal sphere obj.pglobal\n0\n0\n4 %g %g %g %g\n"
   },
   {  "void glow mat.caustic\n0\n0\n4 %e %e %e 0\n",
      "mat.caustic sphere obj.caustic\n0\n0\n4 %g %g %g %g\n"
   },
   {  "void glow mat.volume\n0\n0\n4 %e %e %e 0\n",
      "mat.volume sphere obj.volume\n0\n0\n4 %g %g %g %g\n"
   },
   {  "void glow mat.direct\n0\n0\n4 %e %e %e 0\n",
      "mat.direct sphere obj.direct\n0\n0\n4 %g %g %g %g\n"
   },
   {  "void glow mat.contrib\n0\n0\n4 %e %e %e 0\n",
      "mat.contrib sphere obj.contrib\n0\n0\n4 %g %g %g %g\n"
   }
};

/* Default colour codes are as follows:   global         = blue
                                          precomp global = cyan
                                          caustic        = red
                                          volume         = green
                                          direct         = magenta 
                                          contrib        = yellow */
const COLOR colDefs [] = {
   {0.25, 0.25, 2}, {0.1, 1, 1}, {1, 0.1, 0.1}, 
   {0.1, 1, 0.1}, {1, 0.1, 1}, {1, 1, 0.1}
};


int main (int argc, char** argv)
{
   char           format [MAXFMTLEN];
   RREAL          rad, radScale = RADSCALE, extent, dumpRatio;
   unsigned       arg, j, ptype, dim, fluxCol = 0;
   long           numSpheres = NSPHERES;
   COLOR          customCol = {0, 0, 0};
   FILE           *pmapFile;
   PhotonMap      pm;
   PhotonPrimary  pri;
   Photon         p;
#ifdef PMAP_OOC
   char           leafFname [1024];
#endif

   int setBool(char *str, int pos, int *var)
   {
      switch ((str) [pos]) {
         case '\0': 
            *var = !*var; 
            break;
         case 'y': case 'Y': case 't': case 'T': case '+': case '1': 
            *var = 1; 
            break;
         case 'n': case 'N': case 'f': case 'F': case '-': case '0': 
            *var = 0; 
            break;
         default: 
            return 0;
      }
      
      return 1;
   }

   if (argc < 2) {
      puts("Dump photon maps as RADIANCE scene description\n");
      printf("Usage: %s "
             "[-r radscale1] [-n nspheres1] [-f | -c rcol1 gcol1 bcol1] pmap1 "
             "[-r radscale2] [-n nspheres2] [-f | -c rcol2 gcol2 bcol2] pmap2 "
             "...\n", argv [0]);
      return 1;
   }
   
   for (arg = 1; arg < argc; arg++) {
      /* Parse options */
      if (argv [arg][0] == '-') {
         switch (argv [arg][1]) {
            case 'r':
               if ((radScale = atof(argv [++arg])) <= 0)
                  error(USER, "invalid radius scale");
               break;
               
            case 'n':
               if ((numSpheres = parseMultiplier(argv [++arg])) <= 0)
                  error(USER, "invalid number of spheres");
               break;
               
            case 'c':
               if (fluxCol)
                  error(USER, "-f and -c are mutually exclusive");
               
               if (badarg(argc - arg - 1, &argv [arg + 1], "fff"))
                  error(USER, "invalid RGB colour");
                                 
               for (j = 0; j < 3; j++)
                  customCol [j] = atof(argv [++arg]);
               break;
               
            case 'f':
               if (intens(customCol) > 0)
                  error(USER, "-f and -c are mutually exclusive");
                  
               if (!setBool(argv [arg], 2, &fluxCol))
                  error(USER, "invalid option syntax at -f");
               break;
               
            default:
               sprintf(errmsg, "unknown option %s", argv [arg]);
               error(USER, errmsg);
               return -1;
         }
         
         continue;
      }

      /* Dump photon map */
      if (!(pmapFile = fopen(argv [arg], "rb"))) {
         sprintf(errmsg, "can't open %s", argv [arg]);
         error(SYSTEM, errmsg);
      }

      /* Get format string */
      strcpy(format, PMAP_FORMAT_GLOB);
      if (checkheader(pmapFile, format, NULL) != 1) {
         sprintf(errmsg, "photon map file %s has unknown format %s", 
                 argv [arg], format);
         error(USER, errmsg);
      }

      /* Identify photon map type from format string */
      for (ptype = 0; 
           ptype < NUM_PMAP_TYPES && strcmp(pmapFormat [ptype], format);
           ptype++);

      if (!validPmapType(ptype)) {
         sprintf(errmsg, "file %s contains an unknown photon map type", 
                argv [arg]);
         error(USER, errmsg);
      }

      /* Get file format version and check for compatibility */
      if (strcmp(getstr(format, pmapFile), PMAP_FILEVER))      
         error(USER, "incompatible photon map file format");

      /* Dump command line as comment */
      fputs("# ", stdout);
      printargs(argc, argv, stdout);
      fputc('\n', stdout);

      /* Dump common material def if constant for all photons, 
         i.e. independent of individual flux */
      if (!fluxCol) {
         if (intens(customCol) > 0)
            printf(radDefs [ptype].mat, 
                   customCol [0], customCol [1], customCol [2]);
         else
            printf(radDefs [ptype].mat, colDefs [ptype][0], 
                   colDefs [ptype][1], colDefs [ptype][2]);
         fputc('\n', stdout);
      }
      
      /* Get number of photons */
      pm.numPhotons = getint(sizeof(pm.numPhotons), pmapFile);

      /* Skip avg photon flux */ 
      for (j = 0; j < 3; j++) 
         getflt(pmapFile);

      /* Get distribution extent (min & max photon positions) */
      for (j = 0; j < 3; j++) {
         pm.minPos [j] = getflt(pmapFile);
         pm.maxPos [j] = getflt(pmapFile);
      }
      
      /* Skip centre of gravity, and avg photon dist to it */
      for (j = 0; j < 4; j++)
         getflt(pmapFile);
      
      /* Sphere radius based on avg intersphere dist depending on
         whether the distribution occupies a 1D line (!), a 2D plane, 
         or 3D volume (= sphere distrib density ^-1/d, where d is the
         dimensionality of the distribution) */
      for (j = 0, extent = 1.0, dim = 0; j < 3; j++) {
         rad = pm.maxPos [j] - pm.minPos [j];
         
         if (rad > FTINY) {
            dim++;
            extent *= rad;
         }
      }

      rad = radScale * RADCOEFF * pow(extent / numSpheres, 1./dim);
      
      /* Photon dump probability to satisfy target sphere count */
      dumpRatio = numSpheres < pm.numPhotons 
                  ? (float)numSpheres / pm.numPhotons : 1;
      
      /* Skip primary rays */
      pm.numPrimary = getint(sizeof(pm.numPrimary), pmapFile);
      while (pm.numPrimary-- > 0) {
         /* Skip source index & incident dir */
         getint(sizeof(pri.srcIdx), pmapFile);
#ifdef PMAP_PRIMARYDIR
         /* Skip primary incident dir */
         getint(sizeof(pri.dir), pmapFile);         
#endif
#ifdef PMAP_PRIMARYPOS
         /* Skip primary hitpoint */
         for (j = 0; j < 3; j++)
            getflt(pmapFile);
#endif
      }

#ifdef PMAP_OOC
      /* Open leaf file with filename derived from pmap, replace pmapFile
       * (which is currently the node file) */
      strncpy(leafFname, argv [arg], 1024);
      strncat(leafFname, PMAP_OOC_LEAFSUFFIX, 1024);
      fclose(pmapFile);
      if (!(pmapFile = fopen(leafFname, "rb"))) {
         sprintf(errmsg, "cannot open leaf file %s", leafFname);
         error(SYSTEM, errmsg);
      }
#endif
            
      /* Load photons */      
      while (pm.numPhotons-- > 0) {
#ifdef PMAP_OOC
         /* Get entire photon record from ooC octree leaf file
            !!! OOC PMAP FILES CURRENTLY DON'T USE PORTABLE I/O !!! */
         if (!fread(&p, sizeof(p), 1, pmapFile)) {
            sprintf(errmsg, "error reading OOC leaf file %s", leafFname);
            error(SYSTEM, errmsg);
         }
#else /* kd-tree */
         /* Get photon position */
         for (j = 0; j < 3; j++) 
            p.pos [j] = getflt(pmapFile);

         /* Get photon normal (currently not used) */
         for (j = 0; j < 3; j++) 
            p.norm [j] = getint(1, pmapFile);

         /* Get photon flux */
   #ifdef PMAP_FLOAT_FLUX
         for (j = 0; j < 3; j++) 
            p.flux [j] = getflt(pmapFile);
   #else
         for (j = 0; j < 4; j++) 
            p.flux [j] = getint(1, pmapFile);
   #endif

         /* Skip primary ray index */
         getint(sizeof(p.primary), pmapFile);

         /* Skip flags */
         getint(sizeof(p.flags), pmapFile);
#endif

         /* Dump photon probabilistically acc. to target sphere count */
         if (frandom() <= dumpRatio) {
            if (fluxCol) {
               /* Dump individual material def per photon acc. to flux */
               getPhotonFlux(&p, customCol);
               printf(radDefs [ptype].mat, 
                      customCol [0], customCol [1], customCol [2]);
               fputc('\n', stdout);
            }
            
            printf(radDefs [ptype].obj, p.pos [0], p.pos [1], p.pos [2], rad);
            fputc('\n', stdout);
         }
               
         if (ferror(pmapFile) || feof(pmapFile)) {
            sprintf(errmsg, "error reading %s", argv [arg]);
            error(USER, errmsg);
         }
      }
      
      fclose(pmapFile);
      
      /* Reset defaults for next dump */
      radScale = RADSCALE;
      numSpheres = NSPHERES;
      customCol [0] = customCol [1] = customCol [2] = 0;
      fluxCol = 0;
   }
   
   return 0;
}
