---
title: Writing a Foreign Data Wrapper 
---

This chapter outlines how to write a new foreign-data wrapper.

All operations on a foreign table are handled through its foreign-data wrapper \(FDW\), a library that consists of a set of functions that the core Greenplum Database server calls. The foreign-data wrapper is responsible for fetching data from the remote data store and returning it to the Greenplum Database executor. If updating foreign-data is supported, the wrapper must handle that, too.

The foreign-data wrappers included in the Greenplum Database open source github repository are good references when trying to write your own. You may want to examine the source code for the [file\_fdw](https://github.com/greenplum-db/gpdb/tree/main/contrib/file_fdw) and [postgres\_fdw](https://github.com/greenplum-db/gpdb/tree/main/contrib/postgres_fdw) modules in the `contrib/` directory. The [CREATE FOREIGN DATA WRAPPER](../../ref_guide/sql_commands/CREATE_FOREIGN_DATA_WRAPPER.html) reference page also provides some useful details.

> **Note** The SQL standard specifies an interface for writing foreign-data wrappers. Greenplum Database does not implement that API, however, because the effort to accommodate it into Greenplum would be large, and the standard API hasn't yet gained wide adoption.

This topic includes the following sections:

-   [Requirements](#reqs)
-   [Known Issues and Limitations](#limits)
-   [Header Files](#includes)
-   [Foreign Data Wrapper Functions](#topic2)
-   [Foreign Data Wrapper Callback Functions](#topic3)
-   [Foreign Data Wrapper Helper Functions](#helper)
-   [Greenplum Database Considerations](#topic5)
-   [Building a Foreign Data Wrapper Extension with PGXS](#pkg)
-   [Deployment Considerations](#deployconsider)

**Parent topic:** [Accessing External Data with Foreign Tables](../external/g-foreign.html)

## <a id="reqs"></a>Requirements 

When you develop with the Greenplum Database foreign-data wrapper API:

-   You must develop your code on a system with the same hardware and software architecture as that of your Greenplum Database hosts.
-   Your code must be written in a compiled language such as C, using the version-1 interface. For details on C language calling conventions and dynamic loading, refer to [C Language Functions](https://www.postgresql.org/docs/12/xfunc-c.html) in the PostgreSQL documentation.
-   Symbol names in your object files must not conflict with each other nor with symbols defined in the Greenplum Database server. You must rename your functions or variables if you get error messages to this effect.
-   Review the foreign table introduction described in [Accessing External Data with Foreign Tables](g-foreign.html).

## <a id="limits"></a>Known Issues and Limitations 

The Greenplum Database 6 foreign-data wrapper implementation has the following known issues and limitations:

-   Greenplum Database supports all values of the `mpp_execute` option value for foreign table scans only. Greenplum supports parallel write operations only when `mpp_execute` is set to `'all segments'`; Greenplum initiates write operations through the coordinator for all other `mpp_execute` settings. See [Greenplum Database Considerations](#topic5).

## <a id="includes"></a>Header Files 

The Greenplum Database header files that you may use when you develop a foreign-data wrapper are located in the `greenplum-db/src/include/` directory \(when developing against the Greenplum Database open source github repository\), or installed in the `$GPHOME/include/postgresql/server/` directory \(when developing against a Greenplum installation\):

-   foreign/fdwapi.h - FDW API structures and callback function signatures
-   foreign/foreign.h - foreign-data wrapper helper structs and functions
-   catalog/pg\_foreign\_table.h - foreign table definition
-   catalog/pg\_foreign\_server.h - foreign server definition

Your FDW code may also be dependent on header files and libraries required to access the remote data store.

## <a id="topic2"></a>Foreign Data Wrapper Functions 

The developer of a foreign-data wrapper must implement an SQL-invokable *handler* function, and optionally an SQL-invokable *validator* function. Both functions must be written in a compiled language such as C, using the version-1 interface.

The *handler* function simply returns a struct of function pointers to callback functions that will be called by the Greenplum Database planner, executor, and various maintenance commands. The *handler* function must be registered with Greenplum Database as taking no arguments and returning the special pseudo-type `fdw_handler`. For example:

```
CREATE FUNCTION NEW_fdw_handler()
  RETURNS fdw_handler
  AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
```

Most of the effort in writing a foreign-data wrapper is in implementing the callback functions. The FDW API callback functions, plain C functions that are not visible or callable at the SQL level, are described in [Foreign Data Wrapper Callback Functions](#topic3).

The *validator* function is responsible for validating options provided in `CREATE` and `ALTER` commands for its foreign-data wrapper, as well as foreign servers, user mappings, and foreign tables using the wrapper. The *validator* function must be registered as taking two arguments, a text array containing the options to be validated, and an OID representing the type of object with which the options are associated. For example:

```
CREATE FUNCTION NEW_fdw_validator( text[], oid )
  RETURNS void
  AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
```

The OID argument reflects the type of the system catalog that the object would be stored in, one of `ForeignDataWrapperRelationId`, `ForeignServerRelationId`, `UserMappingRelationId`, or `ForeignTableRelationId`. If no *validator* function is supplied by a foreign data wrapper, Greenplum Database does not check option validity at object creation time or object alteration time.

## <a id="topic3"></a>Foreign Data Wrapper Callback Functions 

The foreign-data wrapper API defines callback functions that Greenplum Database invokes when scanning and updating foreign tables. The API also includes callbacks for performing explain and analyze operations on a foreign table.

The *handler* function of a foreign-data wrapper returns a `palloc`'d `FdwRoutine` struct containing pointers to callback functions described below. The `FdwRoutine` struct is located in the `foreign/fdwapi.h` header file, and is defined as follows:

```
/*
 * FdwRoutine is the struct returned by a foreign-data wrapper's handler
 * function.  It provides pointers to the callback functions needed by the
 * planner and executor.
 *
 * More function pointers are likely to be added in the future.  Therefore
 * it's recommended that the handler initialize the struct with
 * makeNode(FdwRoutine) so that all fields are set to NULL.  This will
 * ensure that no fields are accidentally left undefined.
 */
typedef struct FdwRoutine
{
	NodeTag		type;

	/* Functions for scanning foreign tables */
	GetForeignRelSize_function GetForeignRelSize;
	GetForeignPaths_function GetForeignPaths;
	GetForeignPlan_function GetForeignPlan;
	BeginForeignScan_function BeginForeignScan;
	IterateForeignScan_function IterateForeignScan;
	ReScanForeignScan_function ReScanForeignScan;
	EndForeignScan_function EndForeignScan;

	/*
	 * Remaining functions are optional.  Set the pointer to NULL for any that
	 * are not provided.
	 */

	/* Functions for updating foreign tables */
	AddForeignUpdateTargets_function AddForeignUpdateTargets;
	PlanForeignModify_function PlanForeignModify;
	BeginForeignModify_function BeginForeignModify;
	ExecForeignInsert_function ExecForeignInsert;
	ExecForeignUpdate_function ExecForeignUpdate;
	ExecForeignDelete_function ExecForeignDelete;
	EndForeignModify_function EndForeignModify;
	IsForeignRelUpdatable_function IsForeignRelUpdatable;

	/* Support functions for EXPLAIN */
	ExplainForeignScan_function ExplainForeignScan;
	ExplainForeignModify_function ExplainForeignModify;

	/* Support functions for ANALYZE */
	AnalyzeForeignTable_function AnalyzeForeignTable;
} FdwRoutine;

```

You must implement the scan-related functions in your foreign-data wrapper; implementing the other callback functions is optional.

Scan-related callback functions include:

<table class="table" id="topic3__in201681"><caption></caption><colgroup><col style="width:35.573122529644266%"><col style="width:64.42687747035573%"></colgroup><thead class="thead">
            <tr class="row">
              <th class="entry" id="topic3__in201681__entry__1">Callback Signature</th>
              <th class="entry" id="topic3__in201681__entry__2">Description</th>
            </tr>
          </thead><tbody class="tbody">
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
GetForeignRelSize (PlannerInfo *root,
                   RelOptInfo *baserel,
                   Oid foreigntableid)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Obtain relation size estimates for a foreign table.
                Called at the beginning of planning for a query on a foreign table.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
GetForeignPaths (PlannerInfo *root,
                 RelOptInfo *baserel,
                 Oid foreigntableid)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Create possible access paths for a scan on a
                foreign table. Called during query planning. <div class="note note note_note"><span class="note__title">Note:</span> A Greenplum
                Database-compatible FDW must call
                <code class="ph codeph">create_foreignscan_path()</code> in its
                <code class="ph codeph">GetForeignPaths()</code> callback function.</div></td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>ForeignScan *
GetForeignPlan (PlannerInfo *root,
                RelOptInfo *baserel,
                Oid foreigntableid,
                ForeignPath *best_path,
                List *tlist,
                List *scan_clauses)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Create a <code class="ph codeph">ForeignScan</code> plan node from
                the selected foreign access path. Called at the end of query planning.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
BeginForeignScan (ForeignScanState *node,
                  int eflags)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Begin running a foreign scan. Called during
               executor startup.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>TupleTableSlot *
IterateForeignScan (ForeignScanState *node)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Fetch one row from the foreign source, returning it
                in a tuple table slot; return NULL if no more rows are available.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
ReScanForeignScan (ForeignScanState *node)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Restart the scan from the beginning.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
EndForeignScan (ForeignScanState *node)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">End the scan and release resources.</td>
            </tr>
          </tbody></table>

If a foreign data wrapper supports writable foreign tables, it should provide the update-related callback functions that are required by the capabilities of the FDW. Update-related callback functions include:

<table class="table" id="topic3__in201681"><caption></caption><colgroup><col style="width:35.573122529644266%"><col style="width:64.42687747035573%"></colgroup><thead class="thead">
            <tr class="row">
              <th class="entry" id="topic3__in201681__entry__1">Callback Signature</th>
              <th class="entry" id="topic3__in201681__entry__2">Description</th>
            </tr>
          </thead><tbody class="tbody">
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
AddForeignUpdateTargets (Query *parsetree,
                         RangeTblEntry *target_rte,
                         Relation target_relation)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Add additional information in the foreign
                table that will be retrieved during an update or delete operation 
                to identify the exact row on which to operate.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>List *
PlanForeignModify (PlannerInfo *root,
                   ModifyTable *plan,
                   Index resultRelation,
                   int subplan_index)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Perform additional planning actions required
                for an insert, update, or delete operation on a foreign table,
                and return the information generated.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
BeginForeignModify (ModifyTableState *mtstate,
                    ResultRelInfo *rinfo,
                    List *fdw_private,
                    int subplan_index,
                    int eflags)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Begin executing a modify operation on a
                foreign table. Called during executor startup.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>TupleTableSlot *
ExecForeignInsert (EState *estate,
                   ResultRelInfo *rinfo,
                   TupleTableSlot *slot,
                   TupleTableSlot *planSlot)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Insert a single tuple into the foreign table.
                Return a slot containing the data that was actually inserted, or
                NULL if no row was inserted.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>TupleTableSlot *
ExecForeignUpdate (EState *estate,
                   ResultRelInfo *rinfo,
                   TupleTableSlot *slot,
                   TupleTableSlot *planSlot)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Update a single tuple in the foreign table.
                Return a slot containing the row as it was actually updated, or
                NULL if no row was updated. </td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>TupleTableSlot *
ExecForeignDelete (EState *estate,
                   ResultRelInfo *rinfo,
                   TupleTableSlot *slot,
                   TupleTableSlot *planSlot)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Delete a single tuple from the foreign table.
                Return a slot containing the row that was deleted, or NULL if no
                row was deleted.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>void
EndForeignModify (EState *estate,
                  ResultRelInfo *rinfo)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">End the update and release resources.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic3__in201681__entry__1"><pre class="pre codeblock"><code>int
IsForeignRelUpdatable (Relation rel)</code></pre></td>
              <td class="entry" headers="topic3__in201681__entry__2">Report the update operations supported by the
                specified foreign table.</td>
            </tr>
          </tbody></table>

Refer to [Foreign Data Wrapper Callback Routines](https://www.postgresql.org/docs/12/fdw-callbacks.html) in the PostgreSQL documentation for detailed information about the inputs and outputs of the FDW callback functions.

## <a id="helper"></a>Foreign Data Wrapper Helper Functions 

The FDW API exports several helper functions from the Greenplum Database core server so that authors of foreign-data wrappers have easy access to attributes of FDW-related objects, such as options provided when the user creates or alters the foreign-data wrapper, server, or foreign table. To use these helper functions, you must include `foreign.h` header file in your source file:

```
#include "foreign/foreign.h"
```

The FDW API includes the helper functions listed in the table below. Refer to [Foreign Data Wrapper Helper Functions](https://www.postgresql.org/docs/12/fdw-helpers.html) in the PostgreSQL documentation for more information about these functions.

<table class="table" id="helper__fdw_helper"><caption></caption><colgroup><col style="width:35.573122529644266%"><col style="width:64.42687747035573%"></colgroup><thead class="thead">
            <tr class="row">
              <th class="entry" id="helper__fdw_helper__entry__1">Helper Signature</th>
              <th class="entry" id="helper__fdw_helper__entry__2">Description</th>
            </tr>
          </thead><tbody class="tbody">
            <tr class="row">
              <td class="entry" headers="helper__fdw_helper__entry__1"><pre class="pre codeblock"><code>ForeignDataWrapper *
GetForeignDataWrapper(Oid fdwid);</code></pre></td>
              <td class="entry" headers="helper__fdw_helper__entry__2">Returns the <code class="ph codeph">ForeignDataWrapper</code>
                object for the foreign-data wrapper with the given OID.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="helper__fdw_helper__entry__1"><pre class="pre codeblock"><code>ForeignDataWrapper *
GetForeignDataWrapperByName(const char *name, bool missing_ok);</code></pre></td>
              <td class="entry" headers="helper__fdw_helper__entry__2">Returns the <code class="ph codeph">ForeignDataWrapper</code>
                object for the foreign-data wrapper with the given name.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="helper__fdw_helper__entry__1"><pre class="pre codeblock"><code>ForeignServer *
GetForeignServer(Oid serverid);</code></pre></td>
              <td class="entry" headers="helper__fdw_helper__entry__2">Returns the <code class="ph codeph">ForeignServer</code>
                object for the foreign server with the given OID.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="helper__fdw_helper__entry__1"><pre class="pre codeblock"><code>ForeignServer *
GetForeignServerByName(const char *name, bool missing_ok);</code></pre></td>
              <td class="entry" headers="helper__fdw_helper__entry__2">Returns the <code class="ph codeph">ForeignServer</code>
                object for the foreign server with the given name.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="helper__fdw_helper__entry__1"><pre class="pre codeblock"><code>UserMapping *
GetUserMapping(Oid userid, Oid serverid);</code></pre></td>
              <td class="entry" headers="helper__fdw_helper__entry__2">Returns the <code class="ph codeph">UserMapping</code>
                object for the user mapping of the given role on the given
                server.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="helper__fdw_helper__entry__1"><pre class="pre codeblock"><code>ForeignTable *
GetForeignTable(Oid relid);</code></pre></td>
              <td class="entry" headers="helper__fdw_helper__entry__2">Returns the <code class="ph codeph">ForeignTable</code>
                object for the foreign table with the given OID.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="helper__fdw_helper__entry__1"><pre class="pre codeblock"><code>List *
GetForeignColumnOptions(Oid relid, AttrNumber attnum);</code></pre></td>
              <td class="entry" headers="helper__fdw_helper__entry__2">Returns the per-column FDW options for the
                column with the given foreign table OID and attribute number.</td>
            </tr>
          </tbody></table>

## <a id="topic5"></a>Greenplum Database Considerations 

A Greenplum Database user can specify the `mpp_execute` option when they create or alter a foreign table, foreign server, or foreign data wrapper. A Greenplum Database-compatible foreign-data wrapper examines the `mpp_execute` option value and uses it to determine where to request or send data - from the `master` \(the default\), `any` \(master or any one segment\), or `all segments` (parallel read/write).

Greenplum Database supports all `mpp_execute` settings for a scan.

Greenplum Database supports parallel write when `mpp_execute 'all segments"` is set. For all other `mpp_execute` settings, Greenplum Database executes write/update operations initiated by a foreign data wrapper on the Greenplum coordinator node.

> **Note** When `mpp_execute 'all segments'` is set, Greenplum Database creates the foreign table with a random partition policy. This enables a foreign data wrapper to write to a foreign table from all segments.

The following scan code snippet probes the `mpp_execute` value associated with a foreign table:

```
ForeignTable *table = GetForeignTable(foreigntableid);
if (table->exec_location == FTEXECLOCATION_ALL_SEGMENTS)
{
    ...
}
else if (table->exec_location == FTEXECLOCATION_ANY)
{
    ...
}
else if (table->exec_location == FTEXECLOCATION_COORDINATOR)
{
    ...
} 
```

If the foreign table was not created with an `mpp_execute` option setting, the `mpp_execute` setting of the foreign server, and then the foreign data wrapper, is probed and used. If none of the foreign-data-related objects has an `mpp_execute` setting, the default setting is `master`.

If a foreign-data wrapper supports `mpp_execute 'all'`, it will implement a policy that matches Greenplum segments to data. So as not to duplicate data retrieved from the remote, the FDW on each segment must be able to establish which portion of the data is their responsibility. An FDW may use the segment identifier and the number of segments to help make this determination. The following code snippet demonstrates how a foreign-data wrapper may retrieve the segment number and total number of segments:

```
int segmentNumber = GpIdentity.segindex;
int totalNumberOfSegments = getgpsegmentCount();
```

## <a id="pkg"></a>Building a Foreign Data Wrapper Extension with PGXS 

You compile the foreign-data wrapper functions that you write with the FDW API into one or more shared libraries that the Greenplum Database server loads on demand.

You can use the PostgreSQL build extension infrastructure \(PGXS\) to build the source code for your foreign-data wrapper against a Greenplum Database installation. This framework automates common build rules for simple modules. If you have a more complicated use case, you will need to write your own build system.

To use the PGXS infrastructure to generate a shared library for your FDW, create a simple `Makefile` that sets PGXS-specific variables.

> **Note** Refer to [Extension Building Infrastructure](https://www.postgresql.org/docs/12/extend-pgxs.html) in the PostgreSQL documentation for information about the `Makefile` variables supported by PGXS.

For example, the following `Makefile` generates a shared library in the current working directory named `base_fdw.so` from two C source files, base\_fdw\_1.c and base\_fdw\_2.c:

```
MODULE_big = base_fdw
OBJS = base_fdw_1.o base_fdw_2.o

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

PG_CPPFLAGS = -I$(shell $(PG_CONFIG) --includedir)
SHLIB_LINK = -L$(shell $(PG_CONFIG) --libdir)
include $(PGXS)

```

A description of the directives used in this `Makefile` follows:

-   `MODULE_big` - identifes the base name of the shared library generated by the `Makefile`
-   `PG_CPPFLAGS` - adds the Greenplum Database installation `include/` directory to the compiler header file search path
-   `SHLIB_LINK` adds the Greenplum Database installation library directory \(`$GPHOME/lib/`\) to the linker search path
-   The `PG_CONFIG` and `PGXS` variable settings and the `include` statement are required and typically reside in the last three lines of the `Makefile`.

To package the foreign-data wrapper as a Greenplum Database extension, you create script \(`newfdw--version.sql`\) and control \(`newfdw.control`\) files that register the FDW *handler* and *validator* functions, create the foreign data wrapper, and identify the characteristics of the FDW shared library file.

> **Note** [Packaging Related Objects into an Extension](https://www.postgresql.org/docs/12/extend-extensions.html) in the PostgreSQL documentation describes how to package an extension.

Example foreign-data wrapper extension script file named `base_fdw--1.0.sql`:

```
CREATE FUNCTION base_fdw_handler()
  RETURNS fdw_handler
  AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION base_fdw_validator(text[], oid)
  RETURNS void
  AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER base_fdw
  HANDLER base_fdw_handler
  VALIDATOR base_fdw_validator;
```

Example FDW control file named `base_fdw.control`:

```
# base_fdw FDW extension
comment = 'base foreign-data wrapper implementation; does not do much'
default_version = '1.0'
module_pathname = '$libdir/base_fdw'
relocatable = true
```

When you add the following directives to the `Makefile`, you identify the FDW extension control file base name \(`EXTENSION`\) and SQL script \(`DATA`\):

```
EXTENSION = base_fdw
DATA = base_fdw--1.0.sql
```

Running `make install` with these directives in the `Makefile` copies the shared library and FDW SQL and control files into the specified or default locations in your Greenplum Database installation \(`$GPHOME`\).

## <a id="deployconsider"></a>Deployment Considerations 

You must package the FDW shared library and extension files in a form suitable for deployment in a Greenplum Database cluster. When you construct and deploy the package, take into consideration the following:

-   The FDW shared library must be installed to the same file system location on the coordinator host and on every segment host in the Greenplum Database cluster. You specify this location in the `.control` file. This location is typically the `$GPHOME/lib/postgresql/` directory.
-   The FDW `.sql` and `.control` files must be installed to the `$GPHOME/share/postgresql/extension/` directory on the coordinator host and on every segment host in the Greenplum Database cluster.
-   The `gpadmin` user must have permission to traverse the complete file system path to the FDW shared library file and extension files.

