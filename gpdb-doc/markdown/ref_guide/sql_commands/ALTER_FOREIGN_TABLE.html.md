---
title: ALTER FOREIGN TABLE 
---

Changes the definition of a foreign table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    <action> [, ... ]
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    RENAME [ COLUMN ] <column_name> TO <new_column_name>
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    RENAME TO <new_name>
ALTER FOREIGN TABLE [ IF EXISTS ] <name>
    SET SCHEMA <new_schema>

```

where action is one of:

```

    ADD [ COLUMN ] <column_name> <column_type> [ COLLATE <collation> ] [ <column_constraint> [ ... ] ]
    DROP [ COLUMN ] [ IF EXISTS ] <column_name> [ RESTRICT | CASCADE ]
    ALTER [ COLUMN ] <column_name> [ SET DATA ] TYPE <data_type>
    ALTER [ COLUMN ] <column_name> SET DEFAULT <expression>
    ALTER [ COLUMN ] <column_name> DROP DEFAULT
    ALTER [ COLUMN ] <column_name> { SET | DROP } NOT NULL
    ALTER [ COLUMN ] <column_name> SET STATISTICS <integer>
    ALTER [ COLUMN ] <column_name> SET ( <attribute_option> = <value> [, ... ] )
    ALTER [ COLUMN ] <column_name> RESET ( <attribute_option> [, ... ] )
    ALTER [ COLUMN ] <column_name> OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ])
    DISABLE TRIGGER [ <trigger_name> | ALL | USER ]
    ENABLE TRIGGER [ <trigger_name> | ALL | USER ]
    ENABLE REPLICA TRIGGER <trigger_name>
    ENABLE ALWAYS TRIGGER <trigger_name>
    OWNER TO <new_owner>
    OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ] )
```

## <a id="section3"></a>Description 

`ALTER FOREIGN TABLE` changes the definition of an existing foreign table. There are several subforms of the command:

ADD COLUMN
:   This form adds a new column to the foreign table, using the same syntax as [CREATE FOREIGN TABLE](CREATE_FOREIGN_TABLE.html). Unlike the case when you add a column to a regular table, nothing happens to the underlying storage: this action simply declares that some new column is now accessible through the foreign table.

DROP COLUMN \[ IF EXISTS \]
:   This form drops a column from a foreign table. You must specify `CASCADE` if any objects outside of the table depend on the column; for example, views. If you specify `IF EXISTS` and the column does not exist, no error is thrown. Greenplum Database issues a notice instead.

IF EXISTS
:   If you specify `IF EXISTS` and the foreign table does not exist, no error is thrown. Greenplum Database issues a notice instead.

SET DATA TYPE
:   This form changes the type of a column of a foreign table.

SET/DROP DEFAULT
:   These forms set or remove the default value for a column. Default values apply only in subsequent `INSERT` or `UPDATE` commands; they do not cause rows already in the table to change.

SET/DROP NOT NULL
:   Mark a column as allowing, or not allowing, null values.

SET STATISTICS
:   This form sets the per-column statistics-gathering target for subsequent `ANALYZE` operations. See the similar form of [ALTER TABLE](ALTER_TABLE.html) for more details.

SET \( attribute\_option = value \[, ...\] \] \)
RESET \( attribute\_option \[, ... \] \)
:   This form sets or resets per-attribute options. See the similar form of [ALTER TABLE](ALTER_TABLE.html) for more details.

DISABLE/ENABLE \[ REPLICA \| ALWAYS \] TRIGGER
:   These forms configure the firing of trigger\(s\) belonging to the foreign table. See the similar form of [ALTER TABLE](ALTER_TABLE.html) for more details.

OWNER
:   This form changes the owner of the foreign table to the specified user.

RENAME
:   The `RENAME` forms change the name of a foreign table or the name of an individual column in a foreign table.

SET SCHEMA
:   This form moves the foreign table into another schema.

OPTIONS \( \[ ADD \| SET \| DROP \] option \['value'\] \[, ... \] \)
:   Change options for the foreign table. `ADD`, `SET`, and `DROP` specify the action to perform. If no operation is explicitly specified, the default operation is `ADD`. Option names must be unique. Greenplum Database validates names and values using the server's foreign-data wrapper.

You can combine all of the actions except `RENAME` and `SET SCHEMA` into a list of multiple alterations for Greenplum Database to apply in parallel. For example, it is possible to add several columns and/or alter the type of several columns in a single command.

You must own the table to use `ALTER FOREIGN TABLE`. To change the schema of a foreign table, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the table's schema. \(These restrictions enforce that altering the owner doesn't do anything you couldn't do by dropping and recreating the table. However, a superuser can alter ownership of any table anyway.\) To add a column or to alter a column type, you must also have `USAGE` privilege on the data type.

## <a id="section4"></a>Parameters 

name
:   The name \(possibly schema-qualified\) of an existing foreign table to alter.

column\_name
:   The name of a new or existing column.

new\_column\_name
:   The new name for an existing column.

new\_name
:   The new name for the foreign table.

data\_type
:   The data type of the new column, or new data type for an existing column.

CASCADE
:   Automatically drop objects that depend on the dropped column \(for example, views referencing the column\).

RESTRICT
:   Refuse to drop the column if there are any dependent objects. This is the default behavior.

trigger\_name
:   Name of a single trigger to deactivate or enable.

ALL
:   Deactivate or activate all triggers belonging to the foreign table. \(This requires superuser privilege if any of the triggers are internally generated triggers. The core system does not add such triggers to foreign tables, but add-on code could do so.\)

USER
:   Deactivate or activate all triggers belonging to the foreign table except for internally generated triggers.

new\_owner
:   The user name of the new owner of the foreign table.

new\_schema
:   The name of the schema to which the foreign table will be moved.

## <a id="section5"></a>Notes 

The key word `COLUMN` is noise and can be omitted.

Consistency with the foreign server is not checked when a column is added or removed with `ADD COLUMN` or `DROP COLUMN`, a `NOT NULL` constraint is added, or a column type is changed with `SET DATA TYPE`. It is your responsibility to ensure that the table definition matches the remote side.

Refer to [CREATE FOREIGN TABLE](CREATE_FOREIGN_TABLE.html) for a further description of valid parameters.

## <a id="section6"></a>Examples 

To mark a column as not-null:

```
ALTER FOREIGN TABLE distributors ALTER COLUMN street SET NOT NULL;
```

To change the options of a foreign table:

```
ALTER FOREIGN TABLE myschema.distributors 
    OPTIONS (ADD opt1 'value', SET opt2 'value2', DROP opt3 'value3');
```

## <a id="section7"></a>Compatibility 

The forms `ADD`, `DROP`, and `SET DATA TYPE` conform with the SQL standard. The other forms are Greenplum Database extensions of the SQL standard. The ability to specify more than one manipulation in a single `ALTER FOREIGN TABLE` command is also a Greenplum Database extension.

You can use `ALTER FOREIGN TABLE ... DROP COLUMN` to drop the only column of a foreign table, leaving a zero-column table. This is an extension of SQL, which disallows zero-column foreign tables.

## <a id="section8"></a>See Also 

[ALTER TABLE](ALTER_TABLE.html), [CREATE FOREIGN TABLE](CREATE_FOREIGN_TABLE.html), [DROP FOREIGN TABLE](DROP_FOREIGN_TABLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

