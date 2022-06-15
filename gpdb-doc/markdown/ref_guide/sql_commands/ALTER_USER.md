# ALTER USER 

Changes the definition of a database role \(user\).

## Synopsis 

``` {#sql_command_synopsis}
ALTER USER <name> RENAME TO <newname>

ALTER USER <name> SET <config_parameter> {TO | =} {<value> | DEFAULT}

ALTER USER <name> RESET <config_parameter>

ALTER USER <name> RESOURCE QUEUE {<queue_name> | NONE}

ALTER USER <name> RESOURCE GROUP {<group_name> | NONE}

ALTER USER <name> [ [WITH] <option> [ ... ] ]
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
    | [ENCRYPTED | UNENCRYPTED] PASSWORD '<password>'
    | VALID UNTIL '<timestamp>'
    | [ DENY <deny_point> ]
    | [ DENY BETWEEN <deny_point> AND <deny_point>]
    | [ DROP DENY FOR <deny_point> ]
```

## Description 

`ALTER USER` is an alias for `ALTER ROLE`. See [ALTER ROLE](ALTER_ROLE.html) for more information.

## Compatibility 

The `ALTER USER` statement is a Greenplum Database extension. The SQL standard leaves the definition of users to the implementation.

## See Also 

[ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

