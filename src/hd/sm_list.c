/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 *  sm_list.c
 *   Routines for handling linked generic linked lists, stack, and
 *   circular lists. Data element is an int (can be cast to hold
 *   pointer to more general data element)
 */

#include "standard.h"
#include "sm_list.h" 

/* List of available edges */
static LIST *free_lists = NULL;
#ifdef DEBUG
extern int Malloc_cnt;   
#endif
LIST
*new_list()
{
    LIST *l;

    /* check free list for available edges */
    if( free_lists )
    {
	l = free_lists;
	free_lists = LIST_NEXT(free_lists);
    }
    /* if no free edges- allocate the memory */
    else
    {
      if( !(l = (LIST *)malloc(sizeof(LIST))))
	  error(SYSTEM,"new_list():Unable to allocate memory");
    }
    /* clear the memory */
    bzero(l, sizeof(LIST));

    return(l);
}


/* attaches list a at the end of list b */
LIST
*append_list(a,b)
LIST *a,*b;
{
    LIST * l;

    if(!a)
      return(b);

    if(!b)
       return(a);
    
    l = b;
    while(LIST_NEXT(l) && LIST_NEXT(l) != b)
       l = LIST_NEXT(l);

    SET_LIST_NEXT(l,a);
    
    return(b);
}

/* Adds data to the end of a circular list. If set, "end"
 * is a pointer to the last element in the list
 */
LIST
*add_data_to_circular_list(l,end,d)
LIST *l,**end;
int d;
{
    LIST *list;
    
    list = new_list();
    SET_LIST_DATA(list,d);

    if(!l)
       l = list;
    else
    {   
	if(*end)
	  SET_LIST_NEXT(*end,list);
	else
	   l = append_list(list,l);
    }

    *end = list;
    SET_LIST_NEXT(list,l);
    return(l);
}

/* Pushes data element d at the top of the list- returns pointer
   to new top of list
 */
LIST
*push_data(l,d)
LIST *l;
int d;
{
    LIST * list;
    
    list = new_list();
    SET_LIST_DATA(list,d);
    SET_LIST_NEXT(list,l);
    return(list);
}

/* frees the list */
LIST
*free_list(l)
LIST * l;
{
    if(!l)
       return(NULL);

    free_lists = append_list(free_lists,l);

    return(NULL);
}

/* Returns data element d at the top of the list- returns pointer
   to new top of list
 */
int
pop_list(l)
LIST **l;
{
    LIST *p;
    int d;
    
    if(!l)
       return(NULL);
    if(!(p = *l))
       return(NULL);

    d = LIST_DATA(p);
    
    *l = LIST_NEXT(p);

    LIST_NEXT(p) =NULL;
    free_list(p);
    
    return(d);
}    

/* Search through list for data element "d", and remove from
 *  list: Returns TRUE if found, false otherwise
 */
int
remove_from_list(d,list)
int d;
LIST **list;
{
    LIST *l,*prev;
    
    l = *list;
    prev = NULL;
    
    while(l)
    {
	if(LIST_DATA(l) == d)
	{
	   if(l == *list)
	      *list = LIST_NEXT(*list);
	   else
	      SET_LIST_NEXT(prev,LIST_NEXT(l));
	
	   SET_LIST_NEXT(l,NULL);
	   free_list(l);
	   return(TRUE);
       }
	prev = l;
	l = LIST_NEXT(l);
    }
    return(FALSE);
}










