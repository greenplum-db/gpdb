# ALTER VIEW 

Changes properties of a view.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER VIEW [ IF EXISTS ] <name> ALTER [ COLUMN ] <column_name> SET DEFAULT <expression>

ALTER VIEW [ IF EXISTS ] <name> ALTER [ COLUMN ] <column_name> DROP DEFAULT

ALTER VIEW [ IF EXISTS ] <name> OWNER TO <new_owner>

ALTER VIEW [ IF EXISTS ] <name> RENAME TO <new_name>

ALTER VIEW [ IF EXISTS ] <name> SET SCHEMA <new_schema>

ALTER VIEW [ IF EXISTS ] <name> SET ( <view_option_name> [= <view_option_value>] [, ... ] )

ALTER VIEW [ IF EXISTS ] <name> RESET ( <view_option_name> [, ... ] )
```

## <a id="section3"></a>Description 

`ALTER VIEW` changes various auxiliary properties of a view. \(If you want to modify the view's defining query, use `CREATE OR REPLACE VIEW`.

To run this command you must be the owner of the view. To change a view's schema you must also have `CREATE` privilege on the new schema. To alter the owner, you must also be a direct or indirect member of the new owning role, and that role must have `CREATE` privilege on the view's schema. These restrictions enforce that altering the owner does not do anything you could not do by dropping and recreating the view. However, a superuser can alter ownership of any view.

## <a id="section4"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of an existing view.

`IF EXISTS`
:   Do not throw an error if the view does not exist. A notice is issued in this case.

`SET`/`DROP DEFAULT`
:   These forms set or remove the default value for a column. A view column's default value is substituted into any `INSERT` or `UPDATE` command whose target is the view, before applying any rules or triggers for the view. The view's default will therefore take precedence over any default values from underlying relations.

new\_owner
:   The new owner for the view.

new\_name
:   The new name of the view.

new\_schema
:   The new schema for the view.

`SET ( view\_option\_name [= view\_option\_value] [, ... ] )`
`RESET ( view\_option\_name [, ... ] )`
:   Sets or resets a view option. Currently supported options are:

    `check_option` \(string\)
    :   Changes the check option of the view. The value must be `local` or `cascaded`.

    `security_barrier` \(boolean\)
    :   Changes the security-barrier property of the view. The value must be a Boolean value, such as `true` or `false`.

## <a id="Notes"></a>Notes 

For historical reasons, `ALTER TABLE` can be used with views, too; however, the only variants of `ALTER TABLE` that are allowed with views are equivalent to the statements shown above.

Rename the view `myview` to `newview`:

```
ALTER VIEW myview RENAME TO newview;
```

## <a id="examples"></a>Examples 

To rename the view `foo` to `bar`:

```
ALTER VIEW foo RENAME TO bar;
```

To attach a default column value to an updatable view:

```
CREATE TABLE base_table (id int, ts timestamptz);
CREATE VIEW a_view AS SELECT * FROM base_table;
ALTER VIEW a_view ALTER COLUMN ts SET DEFAULT now();
INSERT INTO base_table(id) VALUES(1);  -- ts will receive a NULL
INSERT INTO a_view(id) VALUES(2);  -- ts will receive the current time
```

## <a id="section6"></a>Compatibility 

`ALTER VIEW` is a Greenplum Database extension of the SQL standard.

## <a id="section7"></a>See Also 

[CREATE VIEW](CREATE_VIEW.html#cj20941), [DROP VIEW](DROP_VIEW.html#dn20941) in the *Greenplum Database Utility Guide*

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

