/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
*  sm_qtree.c: adapted from octree.c from radiance code
*/
/*
 *  octree.c - routines dealing with octrees and cubes.
 *
 *     7/28/85
 */

#include "standard.h"
#include "sm_flag.h"
#include "sm_geom.h"
#include "sm_qtree.h"

QUADTREE  *quad_block[QT_MAX_BLK];		/* our quadtree */
static QUADTREE  quad_free_list = EMPTY;  /* freed octree nodes */
static QUADTREE  treetop = 0;		/* next free node */
int4 *quad_flag= NULL;

#ifdef TEST_DRIVER
extern FVECT Pick_v0[500],Pick_v1[500],Pick_v2[500];
extern int Pick_cnt,Pick_tri,Pick_samp;
extern FVECT Pick_point[500];


#endif
int Incnt=0;

QUADTREE
qtAlloc()			/* allocate a quadtree */
{
    register QUADTREE  freet;

    if ((freet = quad_free_list) != EMPTY)
    {
	quad_free_list = QT_NTH_CHILD(freet, 0);
	return(freet);
    }
    freet = treetop;
    if (QT_BLOCK_INDEX(freet) == 0)
    {
	if (QT_BLOCK(freet) >= QT_MAX_BLK)
	   return(EMPTY);
	if ((quad_block[QT_BLOCK(freet)] = (QUADTREE *)malloc(
			QT_BLOCK_SIZE*4*sizeof(QUADTREE))) == NULL)
	   error(SYSTEM,"qtAlloc(): Unable to allocate memory\n");

	/* Realloc the per/node flags */
	quad_flag = (int4 *)realloc((char *)quad_flag,
			(QT_BLOCK(freet)+1)*((QT_BLOCK_SIZE+7)>>3));
	if (quad_flag == NULL)
	   error(SYSTEM,"qtAlloc(): Unable to allocate memory\n");
    }
    treetop += 4;
    return(freet);
}


qtClearAllFlags()		/* clear all quadtree branch flags */
{
  if (!treetop) 
    return;
  
  /* Clear the node flags*/
  bzero((char *)quad_flag, (QT_BLOCK(treetop-4)+1)*((QT_BLOCK_SIZE+7)>>3));
  /* Clear set flags */
  qtclearsetflags();
}

qtFree(qt)			/* free a quadtree */
   register QUADTREE  qt;
{
	register int  i;

	if (!QT_IS_TREE(qt))
	{
	  qtfreeleaf(qt);
	  return;
	}
	for (i = 0; i < 4; i++)
	   qtFree(QT_NTH_CHILD(qt, i));
	QT_NTH_CHILD(qt, 0) = quad_free_list;
	quad_free_list = qt;
}


qtDone()			/* free EVERYTHING */
{
	register int	i;
	
	qtfreeleaves();
	for (i = 0; i < QT_MAX_BLK; i++)
	{
	    if (quad_block[i] == NULL)
		break;
	    free((char *)quad_block[i]);
	    quad_block[i] = NULL;
	}
	/* Free the flags */
	if (i) free((char *)quad_flag);
	quad_flag = NULL;
	quad_free_list = EMPTY;
	treetop = 0;
}

QUADTREE
qtLocate_leaf(qt,bcoord)
QUADTREE qt;
BCOORD bcoord[3];
{
  int i;

  if(QT_IS_TREE(qt))
  {  
    i = baryi_child(bcoord);

    return(qtLocate_leaf(QT_NTH_CHILD(qt,i),bcoord));
  }
  else
    return(qt);
}

/*
   Return the quadtree node containing pt. It is assumed that pt is in
   the root node qt with ws vertices q0,q1,q2 and plane equation peq.
*/
QUADTREE
qtRoot_point_locate(qt,q0,q1,q2,peq,pt)
QUADTREE qt;
FVECT q0,q1,q2;
FPEQ peq;
FVECT pt;
{
    int i,x,y;
    FVECT i_pt;
    double bcoord[3];
    BCOORD bcoordi[3];

     /* Will return lowest level triangle containing point: It the
       point is on an edge or vertex: will return first associated
       triangle encountered in the child traversal- the others can
       be derived using triangle adjacency information
    */
    if(QT_IS_TREE(qt))
    {  
      /* Find the intersection point */
      intersect_vector_plane(pt,peq,NULL,i_pt);

      x = FP_X(peq);
      y = FP_Y(peq);
      /* Calculate barycentric coordinates of i_pt */
      bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],i_pt[x],i_pt[y],bcoord);
      
      /* convert to integer coordinate */
      convert_dtol(bcoord,bcoordi);

      i = baryi_child(bcoordi);

      return(qtLocate_leaf(QT_NTH_CHILD(qt,i),bcoordi));
    }
    else
      return(qt);
}




/* for triangle v0-v1-v2- returns a,b,c: children are:

	v2                     0: v0,a,c
        /\                     1: a,v1,b                  
       /2 \                    2: c,b,v2
     c/____\b                  3: b,c,a
     /\    /\  
    /0 \3 /1 \
  v0____\/____\v1
        a
 */


qtNth_child_tri(v0,v1,v2,a,b,c,i,r0,r1,r2)
FVECT v0,v1,v2;
FVECT a,b,c;
int i;
FVECT r0,r1,r2;
{

  if(!a)
  {
    /* Caution: r's must not be equal to v's:will be incorrect */
    switch(i){
    case 0:  
      VCOPY(r0,v0);
      EDGE_MIDPOINT_VEC3(r1,v0,v1);
      EDGE_MIDPOINT_VEC3(r2,v2,v0);
      break;
    case 1:  
      EDGE_MIDPOINT_VEC3(r0,v0,v1);
      VCOPY(r1,v1);    
      EDGE_MIDPOINT_VEC3(r2,v1,v2);
      break;
    case 2:  
      EDGE_MIDPOINT_VEC3(r0,v2,v0);
      EDGE_MIDPOINT_VEC3(r1,v1,v2);
      VCOPY(r2,v2);
      break;
    case 3:  
      EDGE_MIDPOINT_VEC3(r0,v1,v2);
      EDGE_MIDPOINT_VEC3(r1,v2,v0);
      EDGE_MIDPOINT_VEC3(r2,v0,v1);
      break;
    }
  }
  else
  {
    switch(i){
    case 0:  
      VCOPY(r0,v0); VCOPY(r1,a);    VCOPY(r2,c);
      break;
    case 1:  
      VCOPY(r0,a); VCOPY(r1,v1);    VCOPY(r2,b);
      break;
    case 2:  
      VCOPY(r0,c); VCOPY(r1,b);    VCOPY(r2,v2);
      break;
    case 3:  
      VCOPY(r0,b); VCOPY(r1,c);    VCOPY(r2,a);
      break;
    }
  }
}

/* Add triangle "id" to all leaf level cells that are children of 
quadtree pointed to by "qtptr" with cell vertices "t1,t2,t3"
that it overlaps (vertex and edge adjacencies do not count
as overlapping). If the addition of the triangle causes the cell to go over
threshold- the cell is split- and the triangle must be recursively inserted
into the new child cells: it is assumed that "v1,v2,v3" are normalized
*/

QUADTREE
qtRoot_add_tri(qt,q0,q1,q2,t0,t1,t2,id,n)
QUADTREE qt;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
int id,n;
{
  if(stri_intersect(q0,q1,q2,t0,t1,t2))
    qt = qtAdd_tri(qt,q0,q1,q2,t0,t1,t2,id,n);

  return(qt);
}

QUADTREE
qtRoot_remove_tri(qt,q0,q1,q2,t0,t1,t2,id,n)
QUADTREE qt;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
int id,n;
{

  if(stri_intersect(q0,q1,q2,t0,t1,t2))
    qt = qtRemove_tri(qt,q0,q1,q2,t0,t1,t2,id,n);
  return(qt);
}


QUADTREE
qtAdd_tri(qt,q0,q1,q2,t0,t1,t2,id,n)
QUADTREE qt;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
int id;
int n;
{
  FVECT a,b,c;
  OBJECT tset[QT_MAXSET+1],*optr,*tptr;
  FVECT r0,r1,r2;
  int i;

  /* if this is tree: recurse */
  if(QT_IS_TREE(qt))
  {
    QT_SET_FLAG(qt);
    n++;
    qtSubdivide_tri(q0,q1,q2,a,b,c);

    if(stri_intersect(t0,t1,t2,q0,a,c))
      QT_NTH_CHILD(qt,0) = qtAdd_tri(QT_NTH_CHILD(qt,0),q0,a,c,t0,t1,t2,id,n);
    if(stri_intersect(t0,t1,t2,a,q1,b))
       QT_NTH_CHILD(qt,1) = qtAdd_tri(QT_NTH_CHILD(qt,1),a,q1,b,t0,t1,t2,id,n);
    if(stri_intersect(t0,t1,t2,c,b,q2))
      QT_NTH_CHILD(qt,2) = qtAdd_tri(QT_NTH_CHILD(qt,2),c,b,q2,t0,t1,t2,id,n);
    if(stri_intersect(t0,t1,t2,b,c,a))
      QT_NTH_CHILD(qt,3) = qtAdd_tri(QT_NTH_CHILD(qt,3),b,c,a,t0,t1,t2,id,n);
    return(qt);
  }
  else
  {
      /* If this leave node emptry- create a new set */
      if(QT_IS_EMPTY(qt))
	qt = qtaddelem(qt,id);
      else
      {
	  /* If the set is too large: subdivide */
	optr = qtqueryset(qt);

	if(QT_SET_CNT(optr) < QT_SET_THRESHOLD)
	  qt = qtaddelem(qt,id);
	else
	{
	  if (n < QT_MAX_LEVELS)
          {
	    /* If set size exceeds threshold: subdivide cell and
	       reinsert set tris into cell
	       */
	    /* CAUTION:If QT_SET_THRESHOLD << QT_MAXSET, and dont add
	       more than a few triangles before expanding: then are safe here
	       otherwise must check to make sure set size is < MAXSET,
	       or  qtgetset could overflow os.
	       */
	    tptr = qtqueryset(qt);
	    if(QT_SET_CNT(tptr) > QT_MAXSET)
	      tptr = (OBJECT *)malloc((QT_SET_CNT(tptr)+1)*sizeof(OBJECT));
	    else
	      tptr = tset;
	    if(!tptr)
	      goto memerr;

	    qtgetset(tptr,qt);	
	    n++;
	    qtfreeleaf(qt);
	    qtSubdivide(qt);
	    qt = qtAdd_tri(qt,q0,q1,q2,t0,t1,t2,id,n);

	    for(optr = QT_SET_PTR(tptr),i = QT_SET_CNT(tptr); i > 0; i--)
	      {
		id = QT_SET_NEXT_ELEM(optr);
		if(!qtTri_from_id(id,r0,r1,r2))
		  continue;
		qt  = qtAdd_tri(qt,q0,q1,q2,r0,r1,r2,id,n);
	      }
	    if(tptr != tset)
	      free(tptr);
	    }
	    else
		qt = qtaddelem(qt,id);
	}
      }
  }
  return(qt);
memerr:
    error(SYSTEM,"qtAdd_tri():Unable to allocate memory");
}


QUADTREE
qtRemove_tri(qt,id,q0,q1,q2,t0,t1,t2)
QUADTREE qt;
int id;
FVECT q0,q1,q2;
FVECT t0,t1,t2;
{
  FVECT a,b,c;
  
  /* test if triangle (t0,t1,t2) overlaps cell triangle (v0,v1,v2) */
  if(!stri_intersect(t0,t1,t2,q0,q1,q2))
    return(qt);

  /* if this is tree: recurse */
  if(QT_IS_TREE(qt))
  {
    qtSubdivide_tri(q0,q1,q2,a,b,c);
    QT_NTH_CHILD(qt,0) = qtRemove_tri(QT_NTH_CHILD(qt,0),id,t0,t1,t2,q0,a,c);
    QT_NTH_CHILD(qt,1) = qtRemove_tri(QT_NTH_CHILD(qt,1),id,t0,t1,t2,a,q1,b);
    QT_NTH_CHILD(qt,2) = qtRemove_tri(QT_NTH_CHILD(qt,2),id,t0,t1,t2,c,b,q2);
    QT_NTH_CHILD(qt,3) = qtRemove_tri(QT_NTH_CHILD(qt,3),id,t0,t1,t2,b,c,a);
    return(qt);
  }
  else
  {
      if(QT_IS_EMPTY(qt))
      {
#ifdef DEBUG	 
	  eputs("qtRemove_tri(): triangle not found\n");
#endif
      }
      /* remove id from set */
      else
      {
	  if(!qtinset(qt,id))
	  {
#ifdef DEBUG	      
	      eputs("qtRemove_tri(): tri not in set\n");
#endif
	  }
	  else
	    qt = qtdelelem(qt,id);
      }
  }
  return(qt);
}


QUADTREE
qtVisit_tri_interior(qt,q0,q1,q2,t0,t1,t2,n0,n1,n2,n,func,f,argptr)
   QUADTREE qt;
   FVECT q0,q1,q2;
   FVECT t0,t1,t2;
   FVECT n0,n1,n2;
   int n;
   int (*func)(),*f;
   int *argptr;
{
    FVECT a,b,c;
    
    /* If qt Flag set, or qt vertices interior to t0t1t2-descend */
tree_modified:

    if(QT_IS_TREE(qt))
    {  
	if(QT_IS_FLAG(qt) ||  point_in_stri_n(n0,n1,n2,q0))
	{
	    QT_SET_FLAG(qt);
	    qtSubdivide_tri(q0,q1,q2,a,b,c);
	    /* descend to children */
	    QT_NTH_CHILD(qt,0) = qtVisit_tri_interior(QT_NTH_CHILD(qt,0),
			          q0,a,c,t0,t1,t2,n0,n1,n2,n+1,func,f,argptr);
	    QT_NTH_CHILD(qt,1) = qtVisit_tri_interior(QT_NTH_CHILD(qt,1),
			          a,q1,b,t0,t1,t2,n0,n1,n2,n+1,func,f,argptr);
	    QT_NTH_CHILD(qt,2) = qtVisit_tri_interior(QT_NTH_CHILD(qt,2),
			          c,b,q2,t0,t1,t2,n0,n1,n2,n+1,func,f,argptr);
	    QT_NTH_CHILD(qt,3) = qtVisit_tri_interior(QT_NTH_CHILD(qt,3),
			          b,c,a,t0,t1,t2,n0,n1,n2,n+1,func,f,argptr);
	}
    }
    else
      if((!QT_IS_EMPTY(qt) && QT_LEAF_IS_FLAG(qt)) ||
	 point_in_stri_n(n0,n1,n2,q0))
      {
	func(&qt,f,argptr,q0,q1,q2,t0,t1,t2,n);
	if(QT_FLAG_IS_MODIFIED(*f))
       {
	 QT_SET_FLAG(qt);
	 goto tree_modified;
       }
	if(QT_IS_LEAF(qt))
	   QT_LEAF_SET_FLAG(qt);
	else
	  if(QT_IS_TREE(qt))
	    QT_SET_FLAG(qt);
      }
    return(qt);
}



int
move_to_nbri(b,db0,db1,db2,tptr)
BCOORD b[3];
BDIR db0,db1,db2;
TINT *tptr;
{
  TINT t,dt;
  BCOORD bc;
  int nbr;
  
  nbr = -1;
  *tptr = 0;
  /* Advance to next node */
  if(b[0]==0 && db0 < 0)
    return(0);
  if(b[1]==0 && db1 < 0)
    return(1);
  if(b[2]==0 && db2 < 0)
    return(2);

  if(db0 < 0)
   {
     bc = b[0]<<SHIFT_MAXBCOORD;
     t = bc/-db0;
     nbr = 0;
   }
  else
    t = HUGET;
  if(db1 < 0)
  {
     bc = b[1] <<SHIFT_MAXBCOORD;
     dt = bc/-db1;
    if( dt < t)
    {
      t = dt;
      nbr = 1;
    }
  }
  if(db2 < 0)
  {
     bc = b[2] << SHIFT_MAXBCOORD;
     dt = bc/-db2;
       if( dt < t)
      {
	t = dt;
	nbr = 2;
      }
    }
  *tptr = t;
  return(nbr);
}

QUADTREE
qtVisit_tri_edges(qt,b,db0,db1,db2,db,wptr,nextptr,t,sign,sfactor,func,f,argptr)
   QUADTREE qt;
   BCOORD b[3];
   BDIR db0,db1,db2,db[3][3];
   int *wptr,*nextptr;
   TINT t[3];
   int sign,sfactor;
   int (*func)();
   int *f,*argptr;
{
    int i,found;
    QUADTREE child;
    int nbr,next,w;
    TINT t_g,t_l,t_i,l;

    if(QT_IS_TREE(qt))
    {
      /* Find the appropriate child and reset the coord */
      i = baryi_child(b);

      QT_SET_FLAG(qt);

      for(;;)
      {
	w = *wptr;
	child = QT_NTH_CHILD(qt,i);
	if(i != 3)
	  QT_NTH_CHILD(qt,i) = 
	    qtVisit_tri_edges(child,b,db0,db1,db2,db,wptr,nextptr,t,sign,
				sfactor+1,func,f,argptr);
	else
	  /* If the center cell- must flip direction signs */
	  QT_NTH_CHILD(qt,i) = 
	    qtVisit_tri_edges(child,b,-db0,-db1,-db2,db,wptr,nextptr,t,1-sign,
				  sfactor+1,func,f,argptr);

	if(QT_FLAG_IS_DONE(*f))
	  return(qt);
	if(*wptr != w)
	{
	  w = *wptr;
	  db0 = db[w][0];db1 = db[w][1];db2 = db[w][2];
	  if(sign)
	    {  db0 *= -1;db1 *= -1; db2 *= -1;}
	}
	/* If in same block: traverse */
	if(i==3)
	  next = *nextptr;
	else
	  if(*nextptr == i)
	    next = 3;
	  else
	 {
	   /* reset the barycentric coordinates in the parents*/
	   baryi_parent(b,i);
	   /* Else pop up to parent and traverse from there */
	   return(qt);
	 }
	baryi_from_child(b,i,next);
	i = next;
      }
    }
    else
   {
     func(&qt,f,argptr);
     if(QT_FLAG_IS_DONE(*f))
     {
       if(!QT_IS_EMPTY(qt))
	 QT_LEAF_SET_FLAG(qt);
       return(qt);
     }
     
     if(!QT_IS_EMPTY(qt))
       QT_LEAF_SET_FLAG(qt);
     /* Advance to next node */
     w = *wptr;
     while(1)
       {
	 nbr = move_to_nbri(b,db0,db1,db2,&t_i);
	 
	 t_g = t_i >> sfactor;
	 	 
	 if(t_g >= t[w])
	 {
	   if(w == 2)
	   {
	     QT_FLAG_SET_DONE(*f);
	     return(qt);
	   }
	   /* The edge fits in the cell- therefore the result of shifting
	      db by sfactor is guaranteed to be less than MAXBCOORD
	      */
	   /* Caution: (t[w]*db) must occur before divide by MAXBCOORD
	      since t[w] will always be < MAXBCOORD
	      */
	   l = t[w] << sfactor;
	   /* NOTE: Change divides to Shift and multiply by sign*/
	   b[0] += (l*db0)/MAXBCOORD;
	   b[1] += (l*db1)/MAXBCOORD;
	   b[2] += (l*db2)/MAXBCOORD;
	   w++;
	   db0 = db[w][0]; db1 = db[w][1]; db2 = db[w][2];
	   if(sign)
	     {  db0 *= -1;db1 *= -1; db2 *= -1;}
	 }
	 else
	 {
	   /* Caution: (t_i*db) must occur before divide by MAXBCOORD
	      since t_i will always be < MAXBCOORD*/
	   /* NOTE: Change divides to Shift and  by sign*/
	   b[0] += (t_i *db0) / MAXBCOORD;
	   b[1] += (t_i *db1) / MAXBCOORD;
	   b[2] += (t_i *db2) / MAXBCOORD;
	   
	   t[w] -= t_g;
	   *wptr = w;
	   *nextptr = nbr;
	   return(qt);
	 }
       }
   }    
}


QUADTREE
qtRoot_visit_tri_edges(qt,q0,q1,q2,peq,tri,i_pt,wptr,nextptr,func,f,argptr)
   QUADTREE qt;
   FVECT q0,q1,q2;
   FPEQ  peq;
   FVECT tri[3],i_pt;
   int *wptr,*nextptr;
   int (*func)();
   int *f,*argptr;
{
  int x,y,z,w,i,j,first;
  QUADTREE child;
  FVECT c,d,v[3];
  double b[4][3],db[3][3],et[3],exit_pt;
  BCOORD bi[3];
  TINT t[3];
  BDIR dbi[3][3];
  
 first =0;
  w = *wptr;
  if(w==-1)
  {
    first = 1;
    *wptr = w = 0;
  }
  /* Project the origin onto the root node plane */

  t[0] = t[1] = t[2] = 0;
  /* Find the intersection point of the origin */
  /* map to 2d by dropping maximum magnitude component of normal */

  x = FP_X(peq);
  y = FP_Y(peq);
  z = FP_Z(peq);
  /* Calculate barycentric coordinates for current vertex */
  if(!first)
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],i_pt[x],i_pt[y],b[3]);  
  else
  /* Just starting: b[0] is the origin point: guaranteed to be valid b*/
  {
    intersect_vector_plane(tri[0],peq,&(et[0]),v[0]);
    bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[0][x],v[0][y],b[0]);  
    VCOPY(b[3],b[0]);
  }

  j = (w+1)%3;
  intersect_vector_plane(tri[j],peq,&(et[j]),v[j]);
  bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[j][x],v[j][y],b[j]);  
  if(et[j] < 0.0)
  {
      VSUB(db[w],b[3],b[j]);
      t[w] = HUGET;
  }
  else
 {
   /* NOTE: for stability: do not increment with ipt- use full dir and
      calculate t: but for wrap around case: could get same problem? 
      */
   VSUB(db[w],b[j],b[3]);
   /* Check if the point is out side of the triangle: if so t[w] =HUGET */
   if((fabs(b[j][0])>(FTINY+1.0)) ||(fabs(b[j][1])>(FTINY+1.0)) ||
      (fabs(b[j][2])>(FTINY+1.0))||(b[j][0] <-FTINY) ||
      (b[j][1]<-FTINY) ||(b[j][2]<-FTINY))
     t[w] = HUGET;
   else
   {
       /* The next point is in the triangle- so db values will be in valid
	  range and t[w]= MAXT
	*/  
       t[w] = MAXT;
       if(j != 0)
	 for(;j < 3;j++)
	 {
	   i = (j+1)%3;
	   if(!first || i != 0)
	   {
	     intersect_vector_plane(tri[i],peq,&(et[i]),v[i]);
	     bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],v[i][x],
		    v[i][y],b[i]);
	   }
	   if(et[i] < 0.0)
	   {
	     VSUB(db[j],b[j],b[i]);
	     t[j] = HUGET;
	     break;
	   }
	   else
	   {
	     VSUB(db[j],b[i],b[j]);
	     if((fabs(b[j][0])>(FTINY+1.0))||(fabs(b[j][1])>(FTINY+1.0)) ||
		(fabs(b[j][2])>(FTINY+1.0))||(b[i][0] <-FTINY) ||
		(b[i][1]<-FTINY) ||(b[i][2]<-FTINY))
	       {
		 t[j] = HUGET;
		 break;
	       }
	     else
	       t[j] = MAXT;
	   }
	 }
   }
 }
  bary_dtol(b[3],db,bi,dbi,t,w);
  
  /* trace the ray starting with this node */
  qt = qtVisit_tri_edges(qt,bi,dbi[w][0],dbi[w][1],dbi[w][2],
		     dbi,wptr,nextptr,t,0,0,func,f,argptr);
  if(!QT_FLAG_IS_DONE(*f))
  {
    b[3][0] = (double)bi[0]/(double)MAXBCOORD;
    b[3][1] = (double)bi[1]/(double)MAXBCOORD;
    b[3][2] = (double)bi[2]/(double)MAXBCOORD;
    i_pt[x] = b[3][0]*q0[x] + b[3][1]*q1[x] + b[3][2]*q2[x];
    i_pt[y] = b[3][0]*q0[y] + b[3][1]*q1[y] + b[3][2]*q2[y];
    i_pt[z] = (-FP_N(peq)[x]*i_pt[x] - FP_N(peq)[y]*i_pt[y]-FP_D(peq))/FP_N(peq)[z]; 
  }
  return(qt);
    
}


QUADTREE
qtRoot_trace_ray(qt,q0,q1,q2,peq,orig,dir,nextptr,func,f,argptr)
   QUADTREE qt;
   FVECT q0,q1,q2;
   FPEQ  peq;
   FVECT orig,dir;
   int *nextptr;
   int (*func)();
   int *f,*argptr;
{
  int x,y,z,nbr,w,i;
  QUADTREE child;
  FVECT v,i_pt;
  double b[2][3],db[3],et[2],d,br[3];
  BCOORD bi[3];
  TINT t[3];
  BDIR dbi[3][3];
  
  /* Project the origin onto the root node plane */
  t[0] = t[1] = t[2] = 0;

  VADD(v,orig,dir);
  /* Find the intersection point of the origin */
  /* map to 2d by dropping maximum magnitude component of normal */
  x = FP_X(peq);
  y = FP_Y(peq);
  z = FP_Z(peq);

  /* Calculate barycentric coordinates for current vertex */
  intersect_vector_plane(orig,peq,&(et[0]),i_pt);
  bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],i_pt[x],i_pt[y],b[0]);  

  intersect_vector_plane(v,peq,&(et[1]),i_pt);
  bary2d(q0[x],q0[y],q1[x],q1[y],q2[x],q2[y],i_pt[x],i_pt[y],b[1]);  
  if(et[1] < 0.0)
    VSUB(db,b[0],b[1]);
  else
   VSUB(db,b[1],b[0]);
  t[0] = HUGET;
  convert_dtol(b[0],bi);
   if(et[1]<0.0 || (fabs(b[1][0])>(FTINY+1.0)) ||(fabs(b[1][1])>(FTINY+1.0)) ||
      (fabs(b[1][2])>(FTINY+1.0))||(b[1][0] <-FTINY) ||
      (b[1][1]<-FTINY) ||(b[1][2]<-FTINY))
     {
       max_index(db,&d);
       for(i=0; i< 3; i++)
	 dbi[0][i] = (BDIR)(db[i]/d*MAXBDIR);
     }
   else
       for(i=0; i< 3; i++)
	 dbi[0][i] = (BDIR)(db[i]*MAXBDIR);
  w=0;
  /* trace the ray starting with this node */
  qt = qtVisit_tri_edges(qt,bi,dbi[0][0],dbi[0][1],dbi[0][2], dbi,&w,
			 nextptr,t,0,0,func,f,argptr);
  if(!QT_FLAG_IS_DONE(*f))
  {
    br[0] = (double)bi[0]/(double)MAXBCOORD;
    br[1] = (double)bi[1]/(double)MAXBCOORD;
    br[2] = (double)bi[2]/(double)MAXBCOORD;
    orig[x] = br[0]*q0[x] + br[1]*q1[x] + br[2]*q2[x];
    orig[y] = br[0]*q0[y] + br[1]*q1[y] + br[2]*q2[y];
    orig[z]=(-FP_N(peq)[x]*orig[x] - 
	     FP_N(peq)[y]*orig[y]-FP_D(peq))/FP_N(peq)[z]; 
  }
  return(qt);
    
}













