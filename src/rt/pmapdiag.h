/* 
   ==================================================================
   Photon map diagnostic output and progress reports

   Roland Schregle (roland.schregle@{hslu.ch, gmail.com})
   (c) Fraunhofer Institute for Solar Energy Systems,
       Lucerne University of Applied Sciences & Arts
   ==================================================================
   
   $Id: pmapdiag.h,v 2.3 2015/04/22 20:28:16 rschregle Exp $
*/
   

#ifndef PMAPDIAG_H
   #define PMAPDIAG_H
   
   #include "platform.h"
   
   #ifdef NON_POSIX
      #ifdef MINGW
         #include  <sys/time.h>
      #endif
   #else
      #ifdef BSD
         #include  <sys/time.h>
         #include  <sys/resource.h>
      #else
         #include  <sys/times.h>
         #include  <unistd.h>
      #endif
   #endif
   
   #include  <time.h>   
   #include  <signal.h>
   

   /* Time at start & last report */
   extern time_t repStartTime, repLastTime;   
   /* Report progress & completion counters */
   extern unsigned long repProgress, repComplete;              


   void pmapDistribReport ();
   /* Report photon distribution progress */

   void pmapPreCompReport ();
   /* Report global photon precomputation progress */
   
   void pmapBiasCompReport (char *stats);   
   /* Append full bias compensation statistics to stats; interface to
    * rpict's report() */
   
#endif