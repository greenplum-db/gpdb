# ALTER DATABASE 

Changes the attributes of a database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER DATABASE <name> [ [WITH] <option> [ ... ]  ]

where <option> can be:

    ALLOW_CONNECTIONS <allowconn>
    CONNECTION LIMIT <connlimit>
    IS_TEMPLATE <istemplate>

ALTER DATABASE <name> RENAME TO <new_name>

ALTER DATABASE <name> OWNER TO { <new_owner> | CURRENT_USER | SESSION_USER }

ALTER DATABASE <name> SET TABLESPACE <new_tablespace>

ALTER DATABASE <name> SET <configuration_parameter> { TO | = } { <value> | DEFAULT }
ALTER DATABASE <name> SET <configuration_parameter> FROM CURRENT
ALTER DATABASE <name> RESET <configuration_parameter>
ALTER DATABASE <name> RESET ALL

```

## <a id="section3"></a>Description 

`ALTER DATABASE` changes the attributes of a database.

The first form changes the per-database settings. \(See below for details.\)  Only the database owner or a superuser can change these settings.

The second form changes the name of the database. Only the database owner or a superuser can rename a database; non-superuser owners must also have the `CREATEDB` privilege. You cannot rename the current database. Connect to a different database first.

The third form changes the owner of the database. To alter the owner, you must own the database and also be a direct or indirect member of the new owning role, and you must have the `CREATEDB` privilege. \(Note that superusers have all these privileges automatically.\)

The fourth form changes the default tablespace of the database. Only the database owner or a superuser can do this; you must also have create privilege for the new tablespace. This command physically moves any tables or indexes in the database's old default tablespace to the new tablespace. The new default tablespace must be empty for this database, and no one can be connected to the database. Note that tables and indexes in non-default tablespaces are not affected.

The remaining forms change the session default for a configuration parameter for a Greenplum database. Whenever a new session is subsequently started in that database, the specified value becomes the session default value. The database-specific default overrides whatever setting is present in the server configuration file \(`postgresql.conf`\). Only the database owner or a superuser can change the session defaults for a database. Certain parameters cannot be set this way, or can only be set by a superuser.

## <a id="section4"></a>Parameters 

name
:   The name of the database whose attributes are to be altered.

allowconn
:   If `false`, then no one can connect to this database.

connlimit
:   The maximum number of concurrent connections allowed to this database on the coordinator. The default is `-1`, no limit. Greenplum Database superusers are exempt from this limit.

istemplate
:   If `true`, then this database can be cloned by any user with `CREATEDB` privileges; if `false`, then only superusers or the owner of the database can clone it. Note that template databases cannot be dropped.

new_name
:   The new name of the database.

new\_owner
:   The new owner of the database.

new\_tablespace
:   The new default tablespace of the database.
:   This form of the command cannot be executed inside a transaction block.

configuration\_parameter value
:   Set this database's session default for the specified configuration parameter to the given value. If value is `DEFAULT` or, equivalently, `RESET` is used, the database-specific setting is removed, so the system-wide default setting will be inherited in new sessions. Use `RESET ALL` to clear all database-specific settings. `SET FROM CURRENT` saves the session's current value of the parameter as the database-specific value.
:   See [SET](SET.html) and [Server Configuration Parameters](../config_params/guc_config.html) for more information about allowed parameter names and values.

## <a id="section5"></a>Notes 

It is also possible to tie a session default to a specific role rather than to a database; see [ALTER ROLE](ALTER_ROLE.html). Role-specific settings override database-specific ones if there is a conflict.

## <a id="section6"></a>Examples 

To disable index scans by default in the `test` database:

```
ALTER DATABASE test SET enable_indexscan TO off;
```

To set the default schema search path for the `mydatabase` database:

```
ALTER DATABASE mydatabase SET search_path TO myschema, public, pg_catalog;
```

## <a id="section7"></a>Compatibility 

The `ALTER DATABASE` statement is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE DATABASE](CREATE_DATABASE.html), [DROP DATABASE](DROP_DATABASE.html), [SET](SET.html), [CREATE TABLESPACE](CREATE_TABLESPACE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

