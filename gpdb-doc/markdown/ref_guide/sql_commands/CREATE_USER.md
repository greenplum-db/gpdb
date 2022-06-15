# CREATE USER 

Defines a new database role with the `LOGIN` privilege by default.

## Synopsis 

``` {#sql_command_synopsis}
CREATE USER <name> [[WITH] <option> [ ... ]]
```

where option can be:

```
      SUPERUSER | NOSUPERUSER
    | CREATEDB | NOCREATEDB
    | CREATEROLE | NOCREATEROLE
    | CREATEUSER | NOCREATEUSER
    | CREATEEXTTABLE | NOCREATEEXTTABLE 
      [ ( <attribute>='<value>'[, ...] ) ]
           where <attributes> and <value> are:
           type='readable'|'writable'
           protocol='gpfdist'|'http'
    | INHERIT | NOINHERIT
    | LOGIN | NOLOGIN
    | CONNECTION LIMIT <connlimit>
    | [ ENCRYPTED | UNENCRYPTED ] PASSWORD '<password>'
    | VALID UNTIL '<timestamp>' 
    | IN ROLE <rolename> [, ...]
    | ROLE <rolename> [, ...]
    | ADMIN <rolename> [, ...]
    | RESOURCE QUEUE <queue_name>
    | RESOURCE GROUP <group_name>
    | [ DENY <deny_point> ]
    | [ DENY BETWEEN <deny_point> AND <deny_point>]
```

## Description 

`CREATE USER` is an alias for [CREATE ROLE](CREATE_ROLE.html).

The only difference between `CREATE ROLE` and `CREATE USER` is that `LOGIN` is assumed by default with `CREATE USER`, whereas `NOLOGIN` is assumed by default with `CREATE ROLE`.

## Compatibility 

There is no `CREATE USER` statement in the SQL standard.

## See Also 

[CREATE ROLE](CREATE_ROLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

