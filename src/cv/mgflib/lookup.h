/* Copyright (c) 1994 Regents of the University of California */

/* SCCSid "$SunId$ LBL" */

/*
 * Header file for table lookup routines
 */

typedef struct {
	char	*key;		/* key name (dynamically allocated) */
	char	*data;		/* pointer to client data */
} LUENT;

typedef struct {
	void	(*freek)();	/* free entry key */
	void	(*freed)();	/* free entry data */
	int	tsiz;		/* current table size */
	LUENT	*tabl;		/* table, if allocated */
	int	ndel;		/* number of deleted entries */
} LUTAB;

#define LU_SINIT(fk,fd)		{(void (*)())(fk),(void (*)())(fd),0,NULL,0}

/*
 * The lu_init routine is called to initialize a table.  The number of
 * elements passed is not a limiting factor, as a table can grow to
 * any size permitted by memory.  However, access will be more efficient
 * if this number strikes a reasonable balance between default memory use
 * and the expected (minimum) table size.  The value returned is the
 * actual allocated table size (or zero if there was insufficient memory).
 *
 * It isn't fully necessary to initialize the LUTAB structure.  If it
 * is cleared (tsiz = 0, tabl = NULL), then the first call to lu_find
 * will allocate a minimal table.  If key and data free functions are
 * to be used, the LU_SINIT macro provides a convenient declaration.
 *
 * The lu_find routine returns the entry corresponding to the given
 * key (any nul-terminated string).  If the entry does not exist, the
 * corresponding key field will be NULL.  If the entry has been
 * previously deleted but not yet freed, then only the data field
 * will be NULL.  It is the caller's responsibility to (allocate and)
 * assign the key and data fields when creating a new entry.
 * The only case where lu_find returns NULL is when the system
 * has run out of memory.
 *
 * The lu_delete routine frees an entry's data (if any) by calling
 * the freed member function, but does not free the key field.  This
 * will be freed later during (or instead of) table reallocation.
 *
 * The lu_done routine calls the given free function once for each
 * assigned table entry (i.e. each entry with an assigned key value).
 * The user must define this routines to free the key and the data
 * in the LU_TAB structure.  The final action of lu_done is to free the
 * allocated table itself.
 */

#ifdef NOPROTO
extern int	lu_init();
extern LUENT	*lu_find();
extern void	lu_delete();
extern void	lu_done();
extern int	lu_hash();
#else
extern int	lu_init(LUTAB *, int);
extern LUENT	*lu_find(LUTAB *, char *);
extern void	lu_delete(LUTAB *, char *);
extern void	lu_done(LUTAB *);
extern int	lu_hash(char *);
#endif
