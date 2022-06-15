# ALTER INDEX 

Changes the definition of an index.

## Synopsis 

``` {#sql_command_synopsis}
ALTER INDEX <name> RENAME TO <new_name>

ALTER INDEX <name> SET TABLESPACE <tablespace_name>

ALTER INDEX <name> SET ( FILLFACTOR = <value> )

ALTER INDEX <name> RESET ( FILLFACTOR )
```

## Description 

`ALTER INDEX` changes the definition of an existing index. There are several subforms:

-   **RENAME** — Changes the name of the index. There is no effect on the stored data.
-   **SET TABLESPACE** — Changes the index's tablespace to the specified tablespace and moves the data file\(s\) associated with the index to the new tablespace. See also `CREATE TABLESPACE`.
-   **SET FILLFACTOR** — Changes the index-method-specific storage parameters for the index. The built-in index methods all accept a single parameter: `FILLFACTOR`. The fillfactor for an index is a percentage that determines how full the index method will try to pack index pages. Index contents will not be modified immediately by this command. Use `REINDEX` to rebuild the index to get the desired effects.
-   **RESET FILLFACTOR** — Resets `FILLFACTOR` to the default. As with `SET`, a `REINDEX` may be needed to update the index entirely.

## Parameters 

name
:   The name \(optionally schema-qualified\) of an existing index to alter.

new\_name
:   New name for the index.

tablespace\_name
:   The tablespace to which the index will be moved.

FILLFACTOR
:   The fillfactor for an index is a percentage that determines how full the index method will try to pack index pages. For B-trees, leaf pages are filled to this percentage during initial index build, and also when extending the index at the right \(largest key values\). If pages subsequently become completely full, they will be split, leading to gradual degradation in the index's efficiency.

:   B-trees use a default fillfactor of 90, but any value from 10 to 100 can be selected. If the table is static then fillfactor 100 is best to minimize the index's physical size, but for heavily updated tables a smaller fillfactor is better to minimize the need for page splits. The other index methods use fillfactor in different but roughly analogous ways; the default fillfactor varies between methods.

## Notes 

These operations are also possible using `ALTER TABLE`.

Changing any part of a system catalog index is not permitted.

## Examples 

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

## Compatibility 

`ALTER INDEX` is a Greenplum Database extension.

## See Also 

[CREATE INDEX](CREATE_INDEX.html), [REINDEX](REINDEX.html), [ALTER TABLE](ALTER_TABLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

