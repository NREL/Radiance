/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * sm_samp.c
 */
#include "standard.h"

#include "sm.h"

RSAMP rsL;


#define SAMPSIZ (3*sizeof(float)+sizeof(int4)+\
			sizeof(TMbright)+6*sizeof(BYTE))
/* Initialize/clear global smL sample list for at least n samples */
int
smAlloc_samples(sm,n,extra_points)
SM *sm;
register int n,extra_points;
{
  unsigned nbytes;
  register unsigned	i;
  RSAMP *rs;

  rs = &rsL;

  SM_SAMP(sm) = rs;

  /* round space up to nearest power of 2 */
  nbytes = n*SAMPSIZ + extra_points*SM_POINTSIZ + 8;
  for (i = 1024; nbytes > i; i <<= 1)
		;
  n = (i - 8 - extra_points*SM_POINTSIZ) / SAMPSIZ;	

  RS_BASE(rs) = (char *)malloc(n*SAMPSIZ + extra_points*SM_POINTSIZ);
  if (!RS_BASE(rs))
    error(SYSTEM,"smInit_samples(): Unable to allocate memory");

  /* assign larger alignment types earlier */
  RS_W_PT(rs) = (float (*)[3])RS_BASE(rs);
  RS_W_DIR(rs) = (int4 *)(RS_W_PT(rs) + n + extra_points);
  RS_BRT(rs) = (TMbright *)(RS_W_DIR(rs) + n + extra_points);
  RS_CHR(rs) = (BYTE (*)[3])(RS_BRT(rs) + n);
  RS_RGB(rs) = (BYTE (*)[3])(RS_CHR(rs) + n);
  RS_MAX_SAMP(rs) = n;
  RS_MAX_AUX_PT(rs) = n + extra_points;

  smClear_samples(sm);

  return(n);
}

smClear_aux_samples(sm)
SM *sm;
{
  RS_NEXT_AUX_PT(SM_SAMP(sm)) = RS_MAX_SAMP(SM_SAMP(sm));
}

int
smClear_samples(sm)
     SM *sm;
{
  RSAMP *samp;

  samp = SM_SAMP(sm);
  RS_FREE_SAMP(samp) = -1;
  RS_REPLACE_SAMP(samp) = 0;
  RS_NUM_SAMP(samp) = 0;
  RS_TONE_MAP(samp) = 0;
  smClear_aux_samples(sm);
  return(1);
}

int
smAdd_aux_point(sm,v,d)
     SM *sm;
     FVECT v,d;
{
    RSAMP *samp;
    int id;
    
    /* Get the SM sample array */    
    samp = SM_SAMP(sm);
    /* Get pointer to next available point */
    id = RS_NEXT_AUX_PT(samp);
    if(id >= RS_MAX_AUX_PT(samp))
       return(-1);

    /* Update free aux pointer */
    RS_NEXT_AUX_PT(samp)++;

    /* Copy vector into point */
    VCOPY(RS_NTH_W_PT(samp,id),v);
    RS_NTH_W_DIR(samp,id) = encodedir(d);
    return(id);
}

void
smInit_sample(sm,id,c,d,p)
   SM *sm;
   int id;
   COLR c;
   FVECT d,p;
{
   RSAMP *samp;

   samp = SM_SAMP(sm);
    if(p)
   {
       /* Copy vector into point */
       VCOPY(RS_NTH_W_PT(samp,id),p);
       RS_NTH_W_DIR(samp,id) = encodedir(d);
   }
    else
       {
	   VADD(RS_NTH_W_PT(samp,id),d,SM_VIEW_CENTER(sm));
	   RS_NTH_W_DIR(samp,id) = -1;
       }
    
#ifndef TEST_DRIVER
    tmCvColrs(&RS_NTH_BRT(samp,id),RS_NTH_CHR(samp,id),c,1);
#else
    if(c)
    {
	RS_NTH_RGB(samp,id)[0] = c[0];
	RS_NTH_RGB(samp,id)[1] = c[1];
	RS_NTH_RGB(samp,id)[2] = c[2];
    }
    else
    { 
	RS_NTH_RGB(samp,id)[0] = 100;
       RS_NTH_RGB(samp,id)[1] = 0;
       RS_NTH_RGB(samp,id)[2] = 0;
    }
#endif
}


void
smReset_sample(sm,id,n_id)
   SM *sm;
   int id,n_id;
{
   RSAMP *samp;

   samp = SM_SAMP(sm);
    
    /* Copy vector into point */
    VCOPY(RS_NTH_W_PT(samp,id),RS_NTH_W_PT(samp,n_id));
    RS_NTH_W_DIR(samp,id) = RS_NTH_W_DIR(samp,n_id);

    RS_NTH_BRT(samp,id) = RS_NTH_BRT(samp,n_id);
    RS_NTH_CHR(samp,id)[0] = RS_NTH_CHR(samp,n_id)[0];
    RS_NTH_CHR(samp,id)[1] = RS_NTH_CHR(samp,n_id)[1];
    RS_NTH_CHR(samp,id)[2] = RS_NTH_CHR(samp,n_id)[2];
    RS_NTH_RGB(samp,id)[0] = RS_NTH_RGB(samp,n_id)[0];
    RS_NTH_RGB(samp,id)[1] = RS_NTH_RGB(samp,n_id)[1];
    RS_NTH_RGB(samp,id)[2] = RS_NTH_RGB(samp,n_id)[2];

}

int
rsAlloc_samp(samp,replaced)
   RSAMP *samp;
   int *replaced;
   
{
    int id;

    if(replaced)
       *replaced = FALSE;
    if(RS_NUM_SAMP(samp) < RS_MAX_SAMP(samp))
    {
	id = RS_NUM_SAMP(samp);
	RS_NUM_SAMP(samp)++;
	
    }
    else
       if(RS_REPLACE_SAMP(samp) != -1)
	{
	    id = RS_REPLACE_SAMP(samp);
	    /* NOTE: NEED to do LRU later*/
	    RS_REPLACE_SAMP(samp) = (id + 1)% RS_MAX_SAMP(samp);
	    if(replaced)
	       *replaced = TRUE;
	}
       else
	  if(RS_FREE_SAMP(samp) != -1)
	     {
		 id = RS_FREE_SAMP(samp);
		 RS_FREE_SAMP(samp) = RS_NTH_W_DIR(samp,id);
	     }
	  else
	     {
#ifdef DEBUG
		 eputs("rsAlloc_samp(): no samples available");
#endif
		 id = -1;
	     }
    
    return(id);
}

int
rsDelete_sample(samp,id)
RSAMP *samp;
int id;
{
    /* First check for a valid id */
    if(id >= RS_MAX_SAMP(samp) || id < 0)
       return(FALSE);
    
    RS_NTH_W_DIR(samp,id) = RS_FREE_SAMP(samp);
    RS_FREE_SAMP(samp) = id;

    return(TRUE);
}

int
smDelete_sample(sm,id)
SM *sm;
int id;
{
    RSAMP *samp;

    samp = SM_SAMP(sm);
    return(rsDelete_sample(samp,id));
}

/* NEEDS: to add free list- and bit for clocked LRU  */
int
smAdd_sample_point(sm,c,dir,p)
     SM *sm;
     COLR c;
     FVECT dir;
     FVECT p;

{
    RSAMP *samp;
    int id;
    int replaced;
    
    /* Get the SM sample array */    
    samp = SM_SAMP(sm);
    /* Get pointer to next available point */
    id = rsAlloc_samp(samp,&replaced);
    if(id < 0)
       return(-1);

    if(replaced)
       smMesh_remove_vertex(sm,id);
    
    smInit_sample(sm,id,c,dir,p);

    return(id);
}









