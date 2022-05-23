# ALTER DATABASE 

Changes the attributes of a database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER DATABASE <name> [ WITH CONNECTION LIMIT <connlimit> ]

ALTER DATABASE <name> RENAME TO <newname>

ALTER DATABASE <name> OWNER TO <new_owner>

ALTER DATABASE <name> SET TABLESPACE <new_tablespace>

ALTER DATABASE <name> SET <parameter> { TO | = } { <value> | DEFAULT }
ALTER DATABASE <name> SET <parameter> FROM CURRENT
ALTER DATABASE <name> RESET <parameter>
ALTER DATABASE <name> RESET ALL

```

## <a id="section3"></a>Description 

`ALTER DATABASE` changes the attributes of a database.

The first form changes the allowed connection limit for a database. Only the database owner or a superuser can change this setting.

The second form changes the name of the database. Only the database owner or a superuser can rename a database; non-superuser owners must also have the `CREATEDB` privilege. You cannot rename the current database. Connect to a different database first.

The third form changes the owner of the database. To alter the owner, you must own the database and also be a direct or indirect member of the new owning role, and you must have the `CREATEDB` privilege. \(Note that superusers have all these privileges automatically.\)

The fourth form changes the default tablespace of the database. Only the database owner or a superuser can do this; you must also have create privilege for the new tablespace. This command physically moves any tables or indexes in the database's old default tablespace to the new tablespace. Note that tables and indexes in non-default tablespaces are not affected.

The remaining forms change the session default for a configuration parameter for a Greenplum database. Whenever a new session is subsequently started in that database, the specified value becomes the session default value. The database-specific default overrides whatever setting is present in the server configuration file \(`postgresql.conf`\). Only the database owner or a superuser can change the session defaults for a database. Certain parameters cannot be set this way, or can only be set by a superuser.

## <a id="section4"></a>Parameters 

name
:   The name of the database whose attributes are to be altered.

connlimit
:   The maximum number of concurrent connections possible. The default of -1 means there is no limitation.

parameter value
:   Set this database's session default for the specified configuration parameter to the given value. If value is `DEFAULT` or, equivalently, `RESET` is used, the database-specific setting is removed, so the system-wide default setting will be inherited in new sessions. Use `RESET ALL` to clear all database-specific settings. See [Server Configuration Parameters](../config_params/guc_config.html) for information about all user-settable configuration parameters.

newname
:   The new name of the database.

new\_owner
:   The new owner of the database.

new\_tablespace
:   The new default tablespace of the database.

## <a id="section5"></a>Notes 

It is also possible to set a configuration parameter session default for a specific role \(user\) rather than to a database. Role-specific settings override database-specific ones if there is a conflict. See `ALTER ROLE`.

## <a id="section6"></a>Examples 

To set the default schema search path for the `mydatabase` database:

```
ALTER DATABASE mydatabase SET search_path TO myschema, 
public, pg_catalog;
```

## <a id="section7"></a>Compatibility 

The `ALTER DATABASE` statement is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE DATABASE](CREATE_DATABASE.html), [DROP DATABASE](DROP_DATABASE.html), [SET](SET.html), [CREATE TABLESPACE](CREATE_TABLESPACE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

