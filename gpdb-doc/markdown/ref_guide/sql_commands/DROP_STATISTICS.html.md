# DROP STATISTICS 

Removes extended statistics.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP STATISTICS [ IF EXISTS ] <name> [, ...] [ CASCADE | RESTRICT ]
```

## <a id="section3"></a>Description

`DROP STATISTICS` removes statistics object\(s\) from the database. Only the statistics object's owner, the schema owner, or a superuser can drop a statistics object.

## <a id="section4"></a>Parameters 

`IF EXISTS`
:   Do not throw an error if the statistics object does not exist. Greenplum Database issues a notice in this case.

name
:   The name \(optionally schema-qualified\) of the statistics object to drop.

CASCADE
RESTRICT
:   These key words have no effect, since there are no dependencies on statistics.

## <a id="section6"></a>Examples 

Destroy two statistics objects in different schemas, without failing if they do not exist:

```
DROP STATISTICS IF EXISTS
    accounting.users_uid_creation,
    public.grants_user_role;
```


## <a id="section7"></a>Compatibility 

There is no `DROP STATISTICS` command in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER STATISTICS](ALTER_STATISTICS.html), [CREATE STATISTICS](CREATE_STATISTICS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

