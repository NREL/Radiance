/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  sm_qtree.h - header file for routines using spherical quadtrees.
 *
 *    adapted from octree.h 
 */

/*
 *	An quadtree is expressed as an integer which is either
 *	an index to 4 other nodes, the empty tree, or an index
 *	to a set of objects.  If the quadtree has a value:
 *
 *		> -1:	it is an index to four other nodes.
 *
 *		-1:	it is empty
 *
 *		< -1:	it is an index to a set of objects
 */

#define  QUADTREE		int

#define  EMPTY		(-1)

#define  QT_IS_EMPTY(qt)	((qt) == EMPTY)
#define  QT_IS_LEAF(qt)	((qt) < EMPTY)
#define  QT_IS_TREE(qt)	((qt) > EMPTY)

#define  QT_INDEX(qt)	       (-(qt)-2)        /* quadtree node from set */
#define  QT_SET_INDEX(i)       (-((i)+2))       /* object set from node */
#define  QT_BLOCK(qt)         ((qt)>>8)	        /* quadtree block index */
#define  QT_BLOCK_INDEX(qt)   (((qt)&0377)<<2)  /* quadtree index in block */

#ifndef  QT_MAX_BLK
#ifdef  BIGMEM
#define  QT_MAX_BLK	65535		/* maximum quadtree block */
#else
#define  QT_MAX_BLK	8191		/* maximum quadtree block */
#endif
#endif



#define  QT_NTH_CHILD(qt,br) (quad_block[QT_BLOCK(qt)][QT_BLOCK_INDEX(qt)+br])
#define  QT_NTH_CHILD_PTR(qt,br) \
                    (&(quad_block[QT_BLOCK(qt)][QT_BLOCK_INDEX(qt)+br]))
#define  QT_CLEAR_CHILDREN(qt) (QT_NTH_CHILD(qt,0)=EMPTY, \
QT_NTH_CHILD(qt,1)=EMPTY,QT_NTH_CHILD(qt,2)=EMPTY,QT_NTH_CHILD(qt,3)=EMPTY)


#if 0
#define  QT_EMPTY	0
#define  QT_FULL	1
#define  QT_TREE	2
#endif



/* OBJECT SET CODE */
#define QT_SET_CNT(s)          ((s)[0])
#define QT_SET_NTH_ELEM(s,n)   ((s)[(n)])   

#define QT_CLEAR_SET(s)        ((s)[0] = 0)
#define QT_SET_NEXT_ELEM(p)    (*(p)++)
#define QT_SET_PTR(s)          (&((s)[1]))


#define QT_MAX_SET        MAXSET
#define MAXCSET           MAXSET*2
#ifndef QT_SET_THRESHOLD
#define QT_SET_THRESHOLD  40  
#endif

#ifndef QT_MAX_LEVELS
#define QT_MAX_LEVELS     16
#endif

#define QT_BLOCK_SIZE 256

extern QUADTREE  qtnewleaf(), qtaddelem(), qtdelelem();

extern QUADTREE  *quad_block[QT_MAX_BLK];	/* quadtree blocks */
