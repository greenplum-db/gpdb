/*-------------------------------------------------------------------------
 *
 * gp_sfv.c
 *
 * Copyright (c) 2010, Greenplum Software
 * Portions Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    gpcontrib/gp_sparse_vector/gp_sfv.c
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include <math.h>

#include "postgres.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "catalog/pg_type.h"
#include "access/tupmacs.h"

#include "sparse_vector.h"

static char **get_text_array_contents(ArrayType *array, int *numitems);
static int gp_isnew_query(void);
static SvecType *classify_document(char **features, int num_features, char **document, int num_words, int allocate);

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


Datum gp_extract_feature_histogram(PG_FUNCTION_ARGS);

/*
 * 	gp_extract_feature_histogram
 * 	By: Luke Lonergan, November 2009, Greenplum Inc.
 * 	Credits:
 * 		This was motivated by discussions with Brian Dolan at FIM / MySpace
 *
 * 	Approach:
 * 		Definitions:
 * 		  Feature Vector:
 * 		  A feature vector is a list of words, generally all of the possible choices of words.
 * 		  In other words, a feature vector is a dictionary and might have cardinality of 20,000 or so.
 *
 * 		  document:
 * 		  A document, here identifed using a list of words. Generally a document will consist of a
 * 		  set of words contained in the feature vector, but sometimes a document will contain words
 * 		  that are not in the feature vector.
 *
 * 		  Sparse Feature Vector (SFV):
 * 		  An SFV is an array of attributes defined for each feature found in a document.  For example,
 * 		  you might define an SFV where each attribute is a count of the number of instances of a
 * 		  feature is found in the document, with one entry per feature found in the document.
 *
 * 		  Example:
 * 		    Say we have a document defined as follows:
 * 		      document1 = {"this","is","an","example","sentence","with","some","some","repeat","repeat"}
 * 		    And say we have a feature vector as follows:
 * 		      features = {"foo","bar","this","is","an","baz","example","sentence","with","some","repeat",
 * 		                  "word1","word2","word3"}
 *
 * 		    Now we'd like to create the SFV for document1.  We can number each feature starting at 1, so
 * 		    that feature(1) = foo, feature(2) = bar and so on.  The SFV of document1 would then be:
 * 		      sfv(document1,features) = {0,0,1,1,1,0,1,1,1,2,2,0,0,0}
 * 		    Note that the position in the SFV array is the number of the feature vector and the attribute
 * 		    is the count of the number of features found in each position.
 *
 * 		  We would like to store the SFV in a terse representation that fits in a small amount of memory.
 * 		  We also want to be able to compare the number of instances where the SFV of one document intersects
 * 		  another.  This routine uses the Sparse Vector datatype to store the SFV.
 *
 * Function Signature is:
 *
 * Where:
 * 	features:		a text array of features (words)
 *	document:		the document, tokenized into words
 *
 * Returns:
 * 	SFV of the document with counts of each feature, stored in a Sparse Vector (svec) datatype
 *
 * TODO:
 * 	Use the built-in hash table structure instead of hsearch()
 * 		The problem with hsearch is that it's not safe to use more than
 * 		one per process.  That means we currently can't do more than one document
 * 		classification per query slice or we'll get the wrong results.
 *	[DONE] Implement a better scheme for detecting whether we're in a new query since
 *	we created the hash table.
 *		Right now we write a key into palloc'ed memory and check to see
 *		if it's the same value on reentry to the classification routine.
 *		This is a hack and may fail in certain circumstances.
 *		A better approach uses the gp_session_id and gp_command_count
 *		to determine if we're in the same query as the last time we were
 *		called.
 */

/*
 * Notes from Brian Dolan on how this feature vector is commonly used:
 *
 * The actual count is hardly ever used.  Insead, it's turned into a weight.  The most
 * common weight is called tf/idf for "Term Frequency / Inverse Document Frequency".
 * The calculation for a given term in a given document is:
 * 	{#Times in this document} * log {#Documents / #Documents  the term appears in}
 * For instance, the term "document" in document A would have weight 1 * log (4/3).  In
 * document D it would have weight 2 * log (4/3).
 * Terms that appear in every document would have tf/idf weight 0, since:
 * 	log (4/4) = log(1) = 0.  (Our example has no term like that.) 
 * That usually sends a lot of values to 0.
 *
 * In this function we're just calculating the term:
 * 	log {#Documents / #Documents  the term appears in}
 * as an Svec.
 *
 * We'll need to have the following arguments:
 * 	Svec *count_corpus           //count of documents in which each feature is found
 * 	float8 *document_count      //count of all documents in corpus
 */

PG_FUNCTION_INFO_V1( gp_extract_feature_histogram );
Datum
gp_extract_feature_histogram(PG_FUNCTION_ARGS)
{

/* Function signature declarations */
  SvecType *returnval;
  char **features, **document;
  int num_features, num_words;

        if (PG_ARGISNULL(0)) PG_RETURN_NULL();

        /* 
         * Perform all the error checking needed to ensure that no one is
         * trying to call this in some sort of crazy way. 
         */
        if (PG_NARGS() != 2) 
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("gp_extract_feature_histogram called with wrong number of arguments"),
					 errhint("Required argument is a base 10 encoded IPv4 address. Example: 10.4.128.1 would be entered here as the number 10004128001.")));

	/* Retrieve the C text array equivalents from the PG text[][] inputs */
	features = get_text_array_contents(PG_GETARG_ARRAYTYPE_P(0),&num_features);
	document   = get_text_array_contents(PG_GETARG_ARRAYTYPE_P(1),&num_words);

/* End of UDF wrapper =================================================== */

#ifdef VERBOSE
	elog(NOTICE,"Number of text items in the feature array is: %d",num_features);
	elog(NOTICE,"Number of text items in the document array is: %d",num_words);
#endif
       	returnval = classify_document(features,num_features,document,num_words,gp_isnew_query());

	pfree(features);
	pfree(document);

	PG_RETURN_POINTER(returnval);
}

static SvecType *
classify_document(char **features, int num_features, char **document, int num_words, int allocate)
{
	float8 *histogram = NULL;
        ENTRY item, *found_item;
	int i;
	SvecType *output_sfv; //Output SFV in a sparse vector datatype

	/*
	 * We need to palloc the histogram array first, since the error cleanup
	 * process on memory pressure allocation failure won't be able to handle
	 * the malloc'ed ordinals array below. Should ordinals fail to allocate
	 * however, then the invoked ereport() will clean up histogram.
	 */
	histogram = (float8 *) palloc0(sizeof(float8) * num_features);

	/*
	 * On saving the state between calls:
	 *
	 * We want to create the hash table one time for each feature set and use it for
	 * all subsequent calls to this routine for efficiency.  However, if another query
	 * executor calls this routine, we don't want the hash table to be left over
	 * from the previous query.
	 *
	 * We know that after each query is executed, the backend associated with the query
	 * is sometimes re-used, so this will leave statically allocated elements around for
	 * reuse, which we can not tolerate.
	 *
	 * However, if we use palloc then the allocations within the default memory context
	 * should be cleared between queries, which allows us to allocate using palloc and
	 * be confident that we won't be sharing those memory allocations between calls.
	 */
	if (allocate) {
		int *ordinals;
#ifdef VERBOSE
		elog(NOTICE,"Classify_document allocating..., Number of features = %d",num_features);
#endif
		/*
		 * Calling hdestroy() isn't guaranteed to free any memory allocated
		 * for the items, the details are implementation specific. Best case
		 * is that the item keys are free'd, while the item data pointers are
		 * never freed. The proper solution here is to move this to using a
		 * dynahash and properly manage the memory, but that remains a TODO
		 * for a rainy day still.
		 */
		hdestroy();
		/* Need to use malloc so that hdestroy() can be called */
		ordinals = (int *) malloc(sizeof(int) * num_features);
		if (!ordinals)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));

		for (i=0; i<num_features; i++) {
			ordinals[i] = i;
		}

		(void) hcreate(num_features);
		for (i=0; i<num_features; i++) {
			if (features[i] != NULL) {
		  		item.key = strdup(features[i]);
				if (!item.key)
				{
					hdestroy();
					free(ordinals);
					ereport(ERROR,
							(errcode(ERRCODE_OUT_OF_MEMORY),
							 errmsg("out of memory")));
				}

				/* If the hash table has the entry already */
				if (hsearch(item, FIND) != NULL)
					free(item.key);
				else
				{
					item.data = (void *)(&(ordinals[i]));

					/*
					 * If the ENTER action returns NULL it means that we are
					 * out of memory and need to error out of this, cleaning
					 * up as best as we can.
					 */
					if (hsearch(item,ENTER) == NULL)
					{
						hdestroy();
						free(item.key);
						free(ordinals);
						ereport(ERROR,
								(errcode(ERRCODE_OUT_OF_MEMORY),
								 errmsg("out of memory")));
					}
				}
			}
		}
	}

	for (i=0;i<num_words;i++) {
		if (document[i] != NULL)
		{
		  item.key = document[i];
		  item.data = NULL;
		  if ((found_item = hsearch(item,FIND)) != NULL) {
			/* Item is in the table */
			histogram[*((int *)found_item->data)]++; //Increment the count at the appropriate ordinal
		  } else {
#ifdef VERBOSE
			elog(NOTICE,"Item not found in feature list %s",(char *)item.key);
#endif
			  continue;
		  }
		}
	}

	/* Create the output SFV as a sparse vector */
	output_sfv = svec_from_float8arr(histogram,num_features);

	return output_sfv;
}

/*
 * Deconstruct a text[] into C-strings (note any NULL elements will be
 * returned as NULL pointers)
 */
static char **
get_text_array_contents(ArrayType *array, int *numitems)
{
        int                     ndim = ARR_NDIM(array);
        int                *dims = ARR_DIMS(array);
        int                     nitems;        int16           typlen;
        bool            typbyval;
        char            typalign;
        char      **values;
        char       *ptr;
        bits8      *bitmap;
        int                     bitmask;
        int                     i;

        Assert(ARR_ELEMTYPE(array) == TEXTOID);

        if (ARR_ELEMTYPE(array) != TEXTOID) {
		*numitems = 0;
		elog(WARNING,"attempt to use a non-text[][] variable with a function that uses text[][] arguments");
		return(NULL);
	}

        *numitems = nitems = ArrayGetNItems(ndim, dims);

        get_typlenbyvalalign(ARR_ELEMTYPE(array),
                                                 &typlen, &typbyval, &typalign);

        values = (char **) palloc(nitems * sizeof(char *));

        ptr = ARR_DATA_PTR(array);
        bitmap = ARR_NULLBITMAP(array);
        bitmask = 1;

        for (i = 0; i < nitems; i++)
        {
                if (bitmap && (*bitmap & bitmask) == 0)
                {
                        values[i] = NULL;
                }
                else
                {
                        values[i] = DatumGetCString(DirectFunctionCall1(textout,
					       	PointerGetDatum(ptr)));
                        ptr = att_addlength_pointer(ptr, typlen, ptr);
                        ptr = (char *) att_align_nominal(ptr, typalign);
                }

                /* advance bitmap pointer if any */
                if (bitmap)
                {
                        bitmask <<= 1;
                        if (bitmask == 0x100)
                        {
                                bitmap++;
                                bitmask = 1;
                        }
                }
        }

        return values;
}
extern int gp_command_count;
extern int gp_session_id;

static int
gp_isnew_query(void)
{
	static int firstcall=1;
	static int last_cnt,last_sid;

	/*
	 * We check command_count and session_id to determine if
	 * we are in a multiple call context.  This allows us to maintain state between
	 * calls.
	 */
	if (firstcall || gp_command_count != last_cnt || gp_session_id != last_sid) {
		last_cnt = gp_command_count;
		last_sid = gp_session_id;
		firstcall = 0;
#ifdef VERBOSE
		elog(NOTICE,"gp_command_count,gp_session_id,last_cnt,last_sid,allocate_new,firstcall = %d,%d,%d,%d,%d,%d",
				gp_command_count,gp_session_id,last_cnt,last_sid,allocate_new,firstcall);
#endif
		return 1;
	} else {
		return 0;
	}
}
