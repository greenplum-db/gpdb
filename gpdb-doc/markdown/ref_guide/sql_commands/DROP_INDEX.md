# DROP INDEX 

Removes an index.

## Synopsis 

``` {#sql_command_synopsis}
DROP INDEX [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## Description 

`DROP INDEX` drops an existing index from the database system. To execute this command you must be the owner of the index.

## Parameters 

IF EXISTS
:   Do not throw an error if the index does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing index.

CASCADE
:   Automatically drop objects that depend on the index.

RESTRICT
:   Refuse to drop the index if any objects depend on it. This is the default.

## Examples 

Remove the index `title_idx`:

```
DROP INDEX title_idx;
```

## Compatibility 

`DROP INDEX` is a Greenplum Database language extension. There are no provisions for indexes in the SQL standard.

## See Also 

[ALTER INDEX](ALTER_INDEX.html), [CREATE INDEX](CREATE_INDEX.html), [REINDEX](REINDEX.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

