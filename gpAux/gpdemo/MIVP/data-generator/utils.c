#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"


float randomrange(float min, float max) {

  float newrand= (float) rand();

  float temp = (max - min) * newrand/(float)RAND_MAX + min;

  return MIN(MAX(temp,min),max);

}


void 
bytebuffer_reset(bytebuffer *dest) 
{
	dest->pos = dest->bytes;
	dest->used_size = 0;
}

void 
bytebuffer_free(bytebuffer *dest) 
{
	if ((dest->bytes) != NULL) 
	{
		free((dest->bytes));
	}
}

void 
bytebuffer_load(char *source, int source_size, bytebuffer *dest) 
{
	/*
	 * Check that (source_size + used_size) fits within the bytebuffer, 
	 * if not, allocate more
	 */
	if ((source_size + dest->used_size) > (dest->alloc_size)) 
	{
		if (! alloc_more_bytebuffer(dest, source_size)) 
		{
			fprintf(stderr,"Failed to allocate memory for bytebuffer! \n");
			exit(1);
		}
	}

	memcpy(dest->pos, source, source_size);

	*(dest->pos+source_size) = '\0'; //Terminate the string

	dest->used_size += source_size;

	dest->pos += source_size;
}


/*
 * alloc_more_bytebuffer() - Memory management function for a line buffer:
 *
 * Allocates memory for a line buffer in chunks of size (bbuf->chunksize).
 *
 * linebuf - If the line buffer pointer passed in is NULL, it will allocate a
 * 		new memory location to it.  If the line buffer point passed in is not NULL,
 * 		realloc() is called to allocate a new buffer with (bbuf->chunksize) more
 * 		room. This also copies the contents of the old linebuffer to the new region.
 *
 * plinebuf_size - is the currently allocated size of linebuf.
 *
 * pplinebuf - is a pointer to a location contained within the linebuffer, and
 * 		is adjusted to refer to the same place in the newly reallocated linebuffer.
 * 		If it is NULL, then it is set to the beginning of the linebuffer.
 *
 * (bbuf->chunksize) - size of allocation chunk in units of sizeof(char)
 *
 * min_size - the minimum size of the buffer needed in bytes
*/
bool 
alloc_more_bytebuffer(bytebuffer *bbuf, int min_size) 
{
	int i;
	int offset=0;
	
	if ((bbuf->alloc_size) >= min_size) 
		return true; //Easy out - we were called without need
	
	/*
	 * Calculate the number of chunks needed.  Add an extra
	 * chunk if the size doesn't chunk evenly.
	 */
	i = min_size/(bbuf->chunksize);
	if ((min_size % (bbuf->chunksize)) != 0) 
		i++;
	
	/* 
	 * In this section, the size of the buffer allocationed or 
	 * reallocationed is extended by one byte to provide for NULL 
	 * termination.
	 */
	if ((bbuf->bytes)==NULL) 
	{
		bbuf->bytes = (char *)malloc(i * (bbuf->chunksize) * sizeof(char) + 1);
		bbuf->pos	= bbuf->bytes;
	} 
	else 
	{
		if ((bbuf->pos)!=NULL)
		{
		   offset = (bbuf->pos) - (bbuf->bytes);
		}
	
		bbuf->bytes = (char *)realloc(bbuf->bytes, i * (bbuf->chunksize) * sizeof(char) + 1); 
	
		if ((bbuf->pos)!=NULL) 
		{
		   bbuf->pos = (bbuf->bytes)+offset;
		}
	}

	bbuf->alloc_size  = i * (bbuf->chunksize);

	// printf("Linebufsize=%d\n",(bbuf->alloc_size)); fflush(stdout);

	return ( (bbuf->bytes) != NULL );
}


/*
 * alloc_more_stringlist() - Memory management function for a stringlist:
 *
 * Allocates memory for a stringlist in chunks of size (slist->chunksize).
 *
 * min_size - the minimum size of the buffer needed in bytes
*/
bool 
alloc_more_stringlist(Stringlist *slist, int min_size) 
{
	int i;
	int offset=0;
	
	if ((slist->alloc_size) >= min_size) 
		return true; //Easy out - we were called without need
	
	/*
	 * Calculate the number of chunks needed.  Add an extra
	 * chunk if the size doesn't chunk evenly.
	 */
	i = min_size/(slist->chunksize);
	if ((min_size % (slist->chunksize)) != 0) 
		i++;
	
	if ((slist->list)==NULL) 
	{
		slist->list = (char **)malloc(i * (slist->chunksize) * sizeof(char *));
		slist->pos	= slist->list;
	} 
	else 
	{
		if ((slist->pos)!=NULL)
		{
		   offset = (slist->pos) - (slist->list);
		}
	
		slist->list = (char **)realloc(slist->list, i * (slist->chunksize) * sizeof(char *)); 
	
		if ((slist->pos)!=NULL) 
		{
		   slist->pos = (slist->list)+offset;
		}
	}

	slist->alloc_size  = i * (slist->chunksize);

	//printf("Stringlist Size=%d\n",(slist->alloc_size)); fflush(stdout);

	return ( (slist->list) != NULL );
}
