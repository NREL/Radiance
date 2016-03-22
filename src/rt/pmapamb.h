/* RCSid $Id$ */
/* 
   ==================================================================
   Photon map interface to RADIANCE ambient calculation
   
   Roland Schregle (roland.schregle@{hslu.ch, gmail.com})
   (c) Fraunhofer Institute for Solar Energy Systems,
   (c) Lucerne University of Applied Sciences and Arts,
   supported by the Swiss National Science Foundation (SNSF, #147053)
   ==================================================================   
   
*/


#ifndef PMAPAMB_H
#define PMAPAMB_H

   #include "pmapdata.h"   

   int ambPmap (COLOR aval, RAY *r, int rdepth);
   /* Factor irradiance from global photon map into ambient coefficient
    * aval; return 1 on success, else 0 (with aval unmodified) */

   int ambPmapCaustic (COLOR aval, RAY *r, int rdepth);
   /* Factor irradiance from caustic photon map into ambient coeffiecient
    * aval; return 1 if successful, else 0 (with aval set to zero) */
#endif
