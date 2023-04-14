# ALTER EXTERNAL TABLE 

Changes the definition of an external table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER EXTERNAL TABLE <name> <action> [, ... ]
```

where action is one of:

```
  ADD [COLUMN] <new_column> <type>
  DROP [COLUMN] <column> [RESTRICT|CASCADE]
  ALTER [COLUMN] <column> TYPE <type>
  OWNER TO <new_owner>
```

## <a id="section3"></a>Description 

`ALTER EXTERNAL TABLE` changes the definition of an existing external table. These are the supported `ALTER EXTERNAL TABLE` actions:

-   **ADD COLUMN** — Adds a new column to the external table definition.
-   **DROP COLUMN** — Drops a column from the external table definition. If you drop readable external table columns, it only changes the table definition in Greenplum Database. The `CASCADE` keyword is required if anything outside the table depends on the column, such as a view that references the column.
-   **ALTER COLUMN TYPE** — Changes the data type of a table column.
-   **OWNER** — Changes the owner of the external table to the specified user.

Use the [ALTER TABLE](ALTER_TABLE.html) command to perform these actions on an external table.

-   Set \(change\) the table schema.
-   Rename the table.
-   Rename a table column.
-   Set \(change\) the distribution policy \(writable external table only\).

You must own the external table to use `ALTER EXTERNAL TABLE` or `ALTER TABLE`. To change the schema of an external table, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the external table's schema. A superuser has these privileges automatically.

Changes to the external table definition with either `ALTER EXTERNAL TABLE` or `ALTER TABLE` do not affect the external data.

The `ALTER EXTERNAL TABLE` and `ALTER TABLE` commands cannot modify the type external table \(read, write, web\), the table `FORMAT` information, or the location of the external data. To modify this information, you must drop and recreate the external table definition.

## <a id="section4"></a>Parameters 

name
:   The name \(possibly schema-qualified\) of an existing external table definition to alter.

column
:   Name of an existing column.

new\_column
:   Name of a new column.

type
:   Data type of the new column, or new data type for an existing column.

new\_owner
:   The role name of the new owner of the external table.

CASCADE
:   Automatically drop objects that depend on the dropped column, such as a view that references the column.

RESTRICT
:   Refuse to drop the column or constraint if there are any dependent objects. This is the default behavior.

## <a id="section5"></a>Examples 

Add a new column to an external table definition:

```
ALTER EXTERNAL TABLE ext_expenses ADD COLUMN manager text;
```

Change the owner of an external table:

```
ALTER EXTERNAL TABLE ext_data OWNER TO jojo;
```

Change the data type of an external table:

```
ALTER EXTERNAL TABLE ext_leads ALTER COLUMN acct_code TYPE integer;
```

## <a id="section6"></a>Compatibility 

`ALTER EXTERNAL TABLE` is a Greenplum Database extension. There is no `ALTER EXTERNAL TABLE` statement in the SQL standard or regular PostgreSQL.

## <a id="section7"></a>See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [DROP EXTERNAL TABLE](DROP_EXTERNAL_TABLE.html), [ALTER TABLE](ALTER_TABLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

