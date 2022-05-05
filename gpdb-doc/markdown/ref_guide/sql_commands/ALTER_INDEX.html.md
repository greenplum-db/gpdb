---
title: ALTER INDEX 
---

Changes the definition of an index.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER INDEX [ IF EXISTS ] <name> RENAME TO <new_name>

ALTER INDEX [ IF EXISTS ] <name> SET TABLESPACE <tablespace_name>

ALTER INDEX [ IF EXISTS ] <name> SET ( <storage_parameter> = <value> [, ...] )

ALTER INDEX [ IF EXISTS ] <name> RESET ( <storage_parameter>  [, ...] )

ALTER INDEX ALL IN TABLESPACE <name> [ OWNED BY <role_name> [, ... ] ]
  SET TABLESPACE <new_tablespace> [ NOWAIT ]

```

## <a id="section3"></a>Description 

`ALTER INDEX` changes the definition of an existing index. There are several subforms:

-   **RENAME** — Changes the name of the index. There is no effect on the stored data.
-   **SET TABLESPACE** — Changes the index's tablespace to the specified tablespace and moves the data file\(s\) associated with the index to the new tablespace. To change the tablespace of an index, you must own the index and have `CREATE` privilege on the new tablespace. All indexes in the current database in a tablespace can be moved by using the `ALL IN TABLESPACE` form, which will lock all indexes to be moved and then move each one. This form also supports `OWNED BY`, which will only move indexes owned by the roles specified. If the `NOWAIT` option is specified then the command will fail if it is unable to acquire all of the locks required immediately. Note that system catalogs will not be moved by this command, use `ALTER DATABASE` or explicit `ALTER INDEX` invocations instead if desired. See also `CREATE TABLESPACE`.
-   **IF EXISTS** — Do not throw an error if the index does not exist. A notice is issued in this case.
-   **SET** — Changes the index-method-specific storage parameters for the index. The built-in index methods all accept a single parameter: fillfactor. The fillfactor for an index is a percentage that determines how full the index method will try to pack index pages. Index contents will not be modified immediately by this command. Use `REINDEX` to rebuild the index to get the desired effects.
-   **RESET** — Resets storage parameters for the index to their defaults. The built-in index methods all accept a single parameter: fillfactor. As with `SET`, a `REINDEX` may be needed to update the index entirely.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing index to alter.

new\_name
:   New name for the index.

tablespace\_name
:   The tablespace to which the index will be moved.

storage\_parameter
:   The name of an index-method-specific storage parameter.

value
:   The new value for an index-method-specific storage parameter. This might be a number or a word depending on the parameter.

## <a id="section5"></a>Notes 

These operations are also possible using `ALTER TABLE`.

Changing any part of a system catalog index is not permitted.

## <a id="section6"></a>Examples 

To rename an existing index:

```
ALTER INDEX distributors RENAME TO suppliers;
```

To move an index to a different tablespace:

```
ALTER INDEX distributors SET TABLESPACE fasttablespace;
```

To change an index's fill factor \(assuming that the index method supports it\):

```
ALTER INDEX distributors SET (fillfactor = 75);
REINDEX INDEX distributors;
```

## <a id="section7"></a>Compatibility 

`ALTER INDEX` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE INDEX](CREATE_INDEX.html), [REINDEX](REINDEX.html), [ALTER TABLE](ALTER_TABLE.html)

**Parent topic:**[SQL Commands](../sql_commands/sql_ref.html)

