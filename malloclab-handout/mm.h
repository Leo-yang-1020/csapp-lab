#include <stdio.h>
#define ARR_SIZE 12
#define CHUNKSIZE (1 << 12)  /* Extended size */
extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);
extern void free_blk_arr_init (void);
void *extend_heap(size_t words);
int delelete_node(void *bp);
void insert_node(char *head, char *bp);
int add_free_blk(char *bp);
void *size2addr(size_t size);
void *find_node(char *list_addr, size_t size);
void *coalesce(void *bp);
void *find_fit(size_t asize);
/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

