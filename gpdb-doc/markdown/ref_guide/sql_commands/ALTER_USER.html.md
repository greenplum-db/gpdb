# ALTER USER 

Changes the definition of a database role \(user\).

## <a id="section2"></a>Synopsis 

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
    | REPLICATION | NOREPLICATION
    | CONNECTION LIMIT <connlimit>
    | [ENCRYPTED | UNENCRYPTED] PASSWORD '<password>'
    | VALID UNTIL '<timestamp>'
    | [ DENY <deny_point> ]
    | [ DENY BETWEEN <deny_point> AND <deny_point>]
    | [ DROP DENY FOR <deny_point> ]
```

## <a id="section3"></a>Description 

`ALTER USER` is an alias for `ALTER ROLE`. See [ALTER ROLE](ALTER_ROLE.html) for more information.

## <a id="section4"></a>Compatibility 

The `ALTER USER` statement is a Greenplum Database extension. The SQL standard leaves the definition of users to the implementation.

## <a id="section5"></a>See Also 

[ALTER ROLE](ALTER_ROLE.html), [CREATE USER](CREATE_USER.html), [DROP USER](DROP_USER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

