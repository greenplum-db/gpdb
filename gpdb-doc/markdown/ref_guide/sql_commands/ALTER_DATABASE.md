# ALTER DATABASE 

Changes the attributes of a database.

## Synopsis 

``` {#sql_command_synopsis}
ALTER DATABASE <name> [ WITH CONNECTION LIMIT <connlimit> ]

ALTER DATABASE <name> SET <parameter> { TO | = } { <value> | DEFAULT }

ALTER DATABASE <name> RESET <parameter>

ALTER DATABASE <name> RENAME TO <newname>

ALTER DATABASE <name> OWNER TO <new_owner>
```

## Description 

`ALTER DATABASE` changes the attributes of a database.

The first form changes the allowed connection limit for a database. Only the database owner or a superuser can change this setting.

The second and third forms change the session default for a configuration parameter for a Greenplum database. Whenever a new session is subsequently started in that database, the specified value becomes the session default value. The database-specific default overrides whatever setting is present in the server configuration file \(`postgresql.conf`\). Only the database owner or a superuser can change the session defaults for a database. Certain parameters cannot be set this way, or can only be set by a superuser.

The fourth form changes the name of the database. Only the database owner or a superuser can rename a database; non-superuser owners must also have the `CREATEDB` privilege. You cannot rename the current database. Connect to a different database first.

The fifth form changes the owner of the database. To alter the owner, you must own the database and also be a direct or indirect member of the new owning role, and you must have the `CREATEDB` privilege. \(Note that superusers have all these privileges automatically.\)

## Parameters 

name
:   The name of the database whose attributes are to be altered.

connlimit
:   The maximum number of concurrent connections possible. The default of -1 means there is no limitation.

parameter value
:   Set this database's session default for the specified configuration parameter to the given value. If value is `DEFAULT` or, equivalently, `RESET` is used, the database-specific setting is removed, so the system-wide default setting will be inherited in new sessions. Use `RESET ALL` to clear all database-specific settings. See [Server Configuration Parameters](../config_params/guc_config.html) for information about server parameters. for information about all user-settable configuration parameters.

newname
:   The new name of the database.

new\_owner
:   The new owner of the database.

## Notes 

It is also possible to set a configuration parameter session default for a specific role \(user\) rather than to a database. Role-specific settings override database-specific ones if there is a conflict. See `ALTER ROLE`.

## Examples 

To set the default schema search path for the `mydatabase` database:

```
ALTER DATABASE mydatabase SET search_path TO myschema, 
public, pg_catalog;
```

## Compatibility 

The `ALTER DATABASE` statement is a Greenplum Database extension.

## See Also 

[CREATE DATABASE](CREATE_DATABASE.html), [DROP DATABASE](DROP_DATABASE.html), [SET](SET.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

