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
#define  QT_IS_LEAF(qt)		((qt) < EMPTY)
#define  QT_IS_TREE(qt)		((qt) > EMPTY)

#define  QT_INDEX(qt)		(-(qt)-2)	/* quadtree node from set */
#define  QT_SET_INDEX(i)	(-((i)+2))	/* object set from node */
#define  QT_BLOCK_SIZE		1024		/* quadtrees in block */
#define  QT_BLOCK(qt)		((qt)>>10)	/* quadtree block index */
#define  QT_BLOCK_INDEX(qt)   (((qt)&0x3ff)<<2)	/* quadtree index in block */

#ifndef  QT_MAX_BLK
#ifdef  BIGMEM
#define  QT_MAX_BLK	16383		/* maximum quadtree block */
#else
#define  QT_MAX_BLK	2047		/* maximum quadtree block */
#endif
#endif


#define  QT_NTH_CHILD(qt,br) (quad_block[QT_BLOCK(qt)][QT_BLOCK_INDEX(qt)+br])
#define  QT_NTH_CHILD_PTR(qt,br) (&QT_NTH_CHILD(qt,br))
#define  QT_CLEAR_CHILDREN(qt) (QT_NTH_CHILD(qt,0)=QT_NTH_CHILD(qt,1)= \
			QT_NTH_CHILD(qt,2)=QT_NTH_CHILD(qt,3)=EMPTY)


/* QUADTREE NODE FLAGS */
#define QT_OFFSET(qt)		((qt)>>5)
#define QT_F_BIT(qt)		((qt)&0x1f)
#define QT_F_OP(f,qt,op)	((f)[QT_OFFSET(qt)] op (0x1<<QT_F_BIT(qt)))
#define QT_IS_FLAG(qt)		QT_F_OP(quad_flag,qt,&)
#define QT_SET_FLAG(qt)		QT_F_OP(quad_flag,qt,|=)
#define QT_CLR_FLAG(qt)		QT_F_OP(quad_flag,qt,|=~)


/* OBJECT SET CODE */
#define QT_SET_CNT(s)          ((s)[0])
#define QT_SET_NTH_ELEM(s,n)   ((s)[(n)])   

#define QT_CLEAR_SET(s)        ((s)[0] = 0)
#define QT_SET_NEXT_ELEM(p)    (*(p)++)
#define QT_SET_PTR(s)          (&((s)[1]))


#define QT_MAX_SET        255
#define MAXCSET           QT_MAX_SET*2
#ifndef QT_SET_THRESHOLD
#define QT_SET_THRESHOLD  30  
#endif

#ifndef QT_MAX_LEVELS
#define QT_MAX_LEVELS     17
#endif

extern QUADTREE  qtnewleaf(), qtaddelem(), qtdelelem();

extern QUADTREE  *quad_block[QT_MAX_BLK];	/* quadtree blocks */
extern int4  *quad_flag;			/* zeroeth quadtree flag */
