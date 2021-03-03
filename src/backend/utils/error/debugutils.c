/*-------------------------------------------------------------------------
 *
 * debugutils.c
 *	  Routines for debugging Greenplum DB
 * 
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/error/debugutils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include <stdio.h>
#include <unistd.h>

#include "nodes/plannodes.h"
#include "utils/debugutils.h"
#include "utils/lsyscache.h"


static void
printatt(unsigned attributeId,
		 Form_pg_attribute attributeP,
		 char *value, char *buf)
{
	sprintf(buf,
			 "\t%2d: %s%s%s%s\t(typeid = %u, len = %d, typmod = %d, byval = %c)|",
		   attributeId,
		   NameStr(attributeP->attname),
		   value != NULL ? " = \"" : "",
		   value != NULL ? value : "",
		   value != NULL ? "\"" : "",
		   (unsigned int) (attributeP->atttypid),
		   attributeP->attlen,
		   attributeP->atttypmod,
		   attributeP->attbyval ? 't' : 'f');
}

/*
 * Return a human readable string representation of a tuple. The returned
 * string must be pfree()d by the caller.
 */

char * 
tup2str(TupleTableSlot *slot)
{
	TupleDesc	typeinfo = slot->tts_tupleDescriptor;
	int			natts = typeinfo->natts;
	int			i;
	Datum		origattr,
				attr;
	char	   *value;
	bool		isnull;
	Oid			typoutput;
	bool		typisvarlena;
	char	   *buf;

	buf = palloc(10 * 1024);
	buf[0] = '\0';

	for (i = 0; i < natts; ++i)
	{
		Form_pg_attribute att = TupleDescAttr(typeinfo, i);
		origattr = slot_getattr(slot, i + 1, &isnull);
		if (isnull)
			continue;
		getTypeOutputInfo(att->atttypid,
						  &typoutput, &typisvarlena);

		/*
		 * If we have a toasted datum, forcibly detoast it here to avoid
		 * memory leakage inside the type's output routine.
		 */
		if (typisvarlena)
			attr = PointerGetDatum(PG_DETOAST_DATUM(origattr));
		else
			attr = origattr;

		value = OidOutputFunctionCall(typoutput, attr);

		printatt((unsigned) i + 1, att, value,
				 &buf[strlen(buf)]);

		pfree(value);

		/* Clean up detoasted copy, if any */
		if (attr != origattr)
			pfree(DatumGetPointer(attr));
	}
	return buf;
}

extern char *plannode_type(Plan *p);

static void
drawnode(FILE *ofile, Plan *plan, List *nodelist)
{
	if(!plan)
		return;

	/* print out the node */
	fprintf(ofile, "\"Node_0x%p\" [\n", plan);
	fprintf(ofile, "label=\"%s %p flow %p\"\n", plannode_type(plan), plan, plan->flow);
	fprintf(ofile, "];\n");

	if(IsA(plan, Append))
	{
		Append *app = (Append *) plan;
		ListCell *cell;
		foreach(cell, app->appendplans)
		{
			Plan *child = (Plan *) lfirst(cell);
			if(!list_member_ptr(nodelist, child))
			{
				nodelist = lappend(nodelist, child);
				drawnode(ofile, child, nodelist);
			}

			fprintf(ofile, "\"Node_0x%p\" -> \"Node_0x%p\" [\n", plan, child);
			fprintf(ofile, "];\n");
		}
	}
	else if (IsA(plan, SubqueryScan))
	{
		SubqueryScan *subq = (SubqueryScan *) plan;
		Plan *child = subq->subplan;
		if(!list_member_ptr(nodelist, child))
		{
			nodelist = lappend(nodelist, child);
			drawnode(ofile, child, nodelist);
		}

		fprintf(ofile, "\"Node_0x%p\" -> \"Node_0x%p\" [\n", plan, child);
		fprintf(ofile, "];\n");
	}
	else
	{
		Plan *child = plan->lefttree;
		if(child)
		{
			if(!list_member_ptr(nodelist, child))
			{
				nodelist = lappend(nodelist, child);
				drawnode(ofile, child, nodelist);
			}

			fprintf(ofile, "\"Node_0x%p\" -> \"Node_0x%p\" [\n", plan, child);
			fprintf(ofile, "];\n");
		}
		child = plan->righttree;
		if(child)
		{
			if(!list_member_ptr(nodelist, child))
			{
				nodelist = lappend(nodelist, child);
				drawnode(ofile, child, nodelist);
			}

			fprintf(ofile, "\"Node_0x%p\" -> \"Node_0x%p\" [\n", plan, child);
			fprintf(ofile, "];\n");
		}
	}
}

/*
 * Write out a plan to be interpretted by dot(1). Tools available at
 * http://www.graphviz.org
 */
void
dotnode(void *node, const char *fname)
{
	List *nodelist = NULL;
	FILE *ofile = fopen(fname, "w+");

	/* Print dot header */

	fprintf(ofile, "digraph g {\n");
	fprintf(ofile, "graph [\n");
	fprintf(ofile, "];\n");

	fprintf(ofile, "node [\n");
	fprintf(ofile, "fontsize = \"14\"\n");
	fprintf(ofile, "shape = \"box\"\n");
	fprintf(ofile, "];\n");
	fprintf(ofile, "edge [\n");
	fprintf(ofile, "fontsize = \"14\"\n");
	fprintf(ofile, "];\n");

	if(node)
	{
		nodelist = lappend(nodelist, node);
		drawnode(ofile, (Plan *) node, nodelist);
	}

	fprintf(ofile, "}\n");
	fclose(ofile);
}

/* 
 * dump a tupledesc
 */
void dump_tupdesc(TupleDesc tupdesc, const char *fname)
{
	FILE *ofile = fopen(fname, "w+");
    int i;

    fprintf(ofile, "TupleDesc: natts %d\n", tupdesc->natts);
    fprintf(ofile, "Name\t\tattlen\tattbyval\tattalign\n");
    fprintf(ofile, "==================================\n");
    for (i=0; i<tupdesc->natts; ++i)
    {
        Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

        fprintf(ofile, "%s, %d, %s, %c\n", 
                    attr->attname.data, attr->attlen,
                    attr->attbyval ? "true" : "false",
                    attr->attalign
               );
    }
    
    fclose(ofile);
}

/* 
 * dump a memtuple binding 
 */
void dump_mt_bind(MemTupleBinding *mt_bind, const char *fname)
{
    FILE *ofile = fopen(fname, "w+");
    int i;

    fprintf(ofile, "Mt_bind: column_align %d, nbm_extra_size %d\n",
            mt_bind->column_align, mt_bind->null_bitmap_extra_size);
    fprintf(ofile, "TupleDesc: natts %d\n",
                    mt_bind->tupdesc->natts);

    fprintf(ofile, " Small binding: vastart %d\n", mt_bind->bind.var_start);
    fprintf(ofile, "Name\t\tattlen\tattbyval\tattalign\toffset\tlen\tflag\tnb\tnm\tns\n");
    fprintf(ofile, "==================================\n");
    for (i=0; i<mt_bind->tupdesc->natts; ++i)
    {
        Form_pg_attribute attr = TupleDescAttr(mt_bind->tupdesc, i); 

        fprintf(ofile, "%s, %d, %s, %c, %d, %d, %d, %d, %d, %d\n", 
                    attr->attname.data, attr->attlen,
                    attr->attbyval ? "true" : "false",
                    attr->attalign,
                    mt_bind->bind.bindings[i].offset,
                    mt_bind->bind.bindings[i].len, 
                    mt_bind->bind.bindings[i].flag, 
                    mt_bind->bind.bindings[i].null_byte, 
                    mt_bind->bind.bindings[i].null_mask, 
                    mt_bind->bind.null_saves[i]
               );
    }
    
    fprintf(ofile, "\n\n Large binding: vastart %d\n", mt_bind->large_bind.var_start);
    fprintf(ofile, "Name\t\tattlen\tattbyval\tattalign\toffset\tlen\tflag\tnb\tnm\tns\n");
    fprintf(ofile, "==================================\n");
    for (i=0; i<mt_bind->tupdesc->natts; ++i)
    {
        Form_pg_attribute attr = TupleDescAttr(mt_bind->tupdesc, i); 

        fprintf(ofile, "%s, %d, %s, %c, %d, %d, %d, %d, %d, %d\n", 
                    attr->attname.data, attr->attlen,
                    attr->attbyval ? "true" : "false",
                    attr->attalign,
                    mt_bind->large_bind.bindings[i].offset,
                    mt_bind->large_bind.bindings[i].len, 
                    mt_bind->large_bind.bindings[i].flag, 
                    mt_bind->large_bind.bindings[i].null_byte, 
                    mt_bind->large_bind.bindings[i].null_mask, 
                    mt_bind->large_bind.null_saves[i]
               );
    }
	fclose(ofile);
}

#ifdef USE_ASSERT_CHECKING
/*
 * Debugging - create/overwrite named file with contents of string.
 *
 * Use, e.g., with gdb to save debugging output.
 */

#include <stdio.h>
#include <string.h>

int debug_write(const char *filename, const char *output_string)
{
	FILE *file;
	
	file = fopen(filename, "w");
	
	if ( !file )
	{
		fprintf(stderr, "debug_write: can't open \"%s\" for output", filename);
		return 1;
	}
	
	if ( fprintf(file, "%s\n", output_string) == EOF )
	{
	    fprintf(stderr, "debug_write: can't write to \"%s\"", filename);
		fclose(file);
		return 1;
	}
	
	fclose(file);
	return 0;
}
#endif

    
    
    
