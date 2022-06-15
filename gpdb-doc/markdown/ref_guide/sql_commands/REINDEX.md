# REINDEX 

Rebuilds indexes.

## Synopsis 

``` {#sql_command_synopsis}
REINDEX {INDEX | TABLE | DATABASE | SYSTEM} <name>
```

## Description 

`REINDEX` rebuilds an index using the data stored in the index's table, replacing the old copy of the index. There are several scenarios in which to use `REINDEX`:

-   An index has become bloated, that is, it contains many empty or nearly-empty pages. This can occur with B-tree indexes in Greenplum Database under certain uncommon access patterns. `REINDEX` provides a way to reduce the space consumption of the index by writing a new version of the index without the dead pages.
-   You have altered the `FILLFACTOR` storage parameter for an index, and wish to ensure that the change has taken full effect.

## Parameters 

INDEX
:   Recreate the specified index.

TABLE
:   Recreate all indexes of the specified table. If the table has a secondary TOAST table, that is reindexed as well.

DATABASE
:   Recreate all indexes within the current database. Indexes on shared system catalogs are skipped. This form of `REINDEX` cannot be executed inside a transaction block.

SYSTEM
:   Recreate all indexes on system catalogs within the current database. Indexes on user tables are not processed. Also, indexes on shared \(global\) system catalogs are skipped. This form of `REINDEX` cannot be executed inside a transaction block.

name
:   The name of the specific index, table, or database to be reindexed. Index and table names may be schema-qualified. Presently, `REINDEX DATABASE` and `REINDEX SYSTEM` can only reindex the current database, so their parameter must match the current database's name.

## Notes 

`REINDEX` is similar to a drop and recreate of the index in that the index contents are rebuilt from scratch. However, the locking considerations are rather different. `REINDEX` locks out writes but not reads of the index's parent table. It also takes an exclusive lock on the specific index being processed, which will block reads that attempt to use that index. In contrast, `DROP INDEX` momentarily takes exclusive lock on the parent table, blocking both writes and reads. The subsequent `CREATE INDEX` locks out writes but not reads; since the index is not there, no read will attempt to use it, meaning that there will be no blocking but reads may be forced into expensive sequential scans.

Reindexing a single index or table requires being the owner of that index or table. Reindexing a database requires being the owner of the database \(note that the owner can therefore rebuild indexes of tables owned by other users\). Of course, superusers can always reindex anything.

`REINDEX` does not update the `reltuples` and `relpages` statistics for the index. To update those statistics, run `ANALYZE` on the table after reindexing.

If you suspect that shared global system catalog indexes are corrupted, they can only be reindexed in Greenplum utility mode. The typical symptom of a corrupt shared index is "index is not a btree" errors, or else the server crashes immediately at startup due to reliance on the corrupted indexes. Contact Greenplum Customer Support for assistance in this situation.

## Examples 

Rebuild a single index:

```
REINDEX INDEX my_index;
```

Rebuild all the indexes on the table `my_table`:

```
REINDEX TABLE my_table;
```

## Compatibility 

There is no `REINDEX` command in the SQL standard.

## See Also 

[CREATE INDEX](CREATE_INDEX.html), [DROP INDEX](DROP_INDEX.html), [VACUUM](VACUUM.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

