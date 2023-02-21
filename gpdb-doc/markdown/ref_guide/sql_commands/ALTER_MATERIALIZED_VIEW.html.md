# ALTER MATERIALIZED VIEW 

Changes the definition of a materialized view.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name> <action> [, ... ]
ALTER MATERIALIZED VIEW <name>
    DEPENDS ON EXTENSION <extension_name>
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name>
    RENAME [ COLUMN ] <column_name> TO <new_column_name>
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name>
    RENAME TO <new_name>
ALTER MATERIALIZED VIEW [ IF EXISTS ] <name>
    SET SCHEMA <new_schema>
ALTER MATERIALIZED VIEW ALL IN TABLESPACE <name> [ OWNED BY <role_name> [, ... ] ]
    SET TABLESPACE <new_tablespace> [ NOWAIT ]

where <action> is one of:

    ALTER [ COLUMN ] <column_name> SET STATISTICS <integer>
    ALTER [ COLUMN ] <column_name> SET ( <attribute_option> = <value> [, ... ] )
    ALTER [ COLUMN ] <column_name> RESET ( <attribute_option> [, ... ] )
    ALTER [ COLUMN ] <column_name> SET STORAGE { PLAIN | EXTERNAL | EXTENDED | MAIN }
    CLUSTER ON <index_name>
    SET WITHOUT CLUSTER
    SET TABLESPACE <new_tablespace>
    SET ( <storage_paramete>r = <value> [, ... ] )
    RESET ( <storage_parameter> [, ... ] )
    OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }
```

## <a id="section3"></a>Description 

`ALTER MATERIALIZED VIEW` changes various auxiliary properties of an existing materialized view.

You must own the materialized view to use `ALTER MATERIALIZED VIEW`. To change a materialized view's schema, you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the materialized view's schema. \(These restrictions enforce that altering the owner doesn't do anything you couldn't do by dropping and recreating the materialized view. However, a superuser can alter ownership of any view anyway.\)

The `DEPENDS ON EXTENSION` form marks the materialized view as dependent on an extension, such that the materialized view will automatically be dropped if the extension is dropped.

The statement subforms and actions available for `ALTER MATERIALIZED VIEW` are a subset of those available for `ALTER TABLE`, and have the same meaning when used for materialized views. See the descriptions for [ALTER TABLE](ALTER_TABLE.html) for details.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing materialized view.

column\_name
:   Name of a new or existing column.

extension\_name
:   The name of the extension that the materialized view is to depend on.

new\_column\_name
:   New name for an existing column.

new\_owner
:   The user name of the new owner of the materialized view.

new\_name
:   The new name for the materialized view.

new\_schema
:   The new schema for the materialized view.

## <a id="section6"></a>Examples 

To rename the materialized view `foo` to `bar`:

```
ALTER MATERIALIZED VIEW foo RENAME TO bar;
```

## <a id="section7"></a>Compatibility 

`ALTER MATERIALIZED VIEW` is a Greenplum Database extension of the SQL standard.

## <a id="section8"></a>See Also 

[CREATE MATERIALIZED VIEW](CREATE_MATERIALIZED_VIEW.html), [DROP MATERIALIZED VIEW](DROP_MATERIALIZED_VIEW.html), [REFRESH MATERIALIZED VIEW](REFRESH_MATERIALIZED_VIEW.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

