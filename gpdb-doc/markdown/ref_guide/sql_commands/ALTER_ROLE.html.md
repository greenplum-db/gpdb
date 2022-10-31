# ALTER ROLE 

Changes a database role \(user or group\).

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER ROLE <role_specification> [ WITH ] <option> [ ... ]

where <option> can be:

    SUPERUSER | NOSUPERUSER
  | CREATEDB | NOCREATEDB
  | CREATEROLE | NOCREATEROLE
  | CREATEEXTTABLE | NOCREATEEXTTABLE  [ ( attribute='value' [, ...] )
     where attributes and values are:
       type='readable'|'writable'
       protocol='gpfdist'|'http'
  | INHERIT | NOINHERIT
  | LOGIN | NOLOGIN
  | REPLICATION | NOREPLICATION
  | BYPASSRLS | NOBYPASSRLS
  | CONNECTION LIMIT <connlimit>
  | [ ENCRYPTED ] PASSWORD '<password>' | PASSWORD NULL
  | VALID UNTIL '<timestamp>'
  | [ DENY <deny_point> ]
  | [ DENY BETWEEN <deny_point> AND <deny_point>]
  | [ DROP DENY FOR <deny_point> ]

ALTER ROLE <name> RENAME TO <new_name>

ALTER ROLE { <role_specification> | ALL } [ IN DATABASE <database_name> ] SET <configuration_parameter> { TO | = } { <value> | DEFAULT }
ALTER ROLE { <role_specification> | ALL } [ IN DATABASE <database_name> ] SET <configuration_parameter> FROM CURRENT
ALTER ROLE { <role_specification> | ALL } [ IN DATABASE <database_name> ] RESET <configuration_parameter>
ALTER ROLE { <role_specification> | ALL } [ IN DATABASE <database_name> ] RESET ALL

ALTER ROLE <role_specification> RESOURCE QUEUE {<queue_name> | NONE}
ALTER ROLE <role_specification> RESOURCE GROUP {<group_name> | NONE}

where <role_specification> can be:

    <role_name>
  | CURRENT_USER
  | SESSION_USER
```

## <a id="section3"></a>Description 

`ALTER ROLE` changes the attributes of a Greenplum Database role. There are several variants of this command.

**WITH option**
:   Changes many of the role attributes that can be specified in [CREATE ROLE](CREATE_ROLE.html). \(All of the possible attributes are covered, execept that there are no options for adding or removing memberships; use [GRANT](GRANT.html) and [REVOKE](REVOKE.html) for that.\) Attributes not mentioned in the command retain their previous settings. Database superusers can change any of these settings for any role. Roles having `CREATEROLE` privilege can change any of these settings except `SUPERUSER`, `REPLICATION`, and `BYPASSRLS`, but only for non-superuser and non-replication roles. Ordinary roles can only change their own password.

**RENAME**
:   Changes the name of the role. Database superusers can rename any role. Roles having `CREATEROLE` privilege can rename non-superuser roles. The current session user cannot be renamed \(connect as a different user to rename a role\). Because MD5-encrypted passwords use the role name as cryptographic salt, renaming a role clears its password if the password is MD5-encrypted.

**SET \| RESET**
:   Changes a role's session default for a specified configuration parameter, either for all databases or, when the `IN DATABASE` clause is specified, only for sessions in the named database. If `ALL` is specified instead of a role name, this changes the setting for all roles. Using `ALL` with `IN DATABASE` is effectively the same as using the command `ALTER DATABASE ... SET ...`.

    Whenever the role subsequently starts a new session, the specified value becomes the session default, overriding whatever setting is present in the server configuration file \(`postgresql.conf`\) or has been received from the `postgres` command line. This only happens at login time; running [SET ROLE](SET_ROLE.html) or [SET SESSION AUTHORIZATION](SET_SESSION_AUTHORIZATION.html) does not cause new configuration values to be set. Settings set for all databases are overridden by database-specific settings attached to a role. Settings for specific databases or specific roles override settings for all roles.

    Superusers can change anyone's session defaults. Roles having `CREATEROLE` privilege can change defaults for non-superuser roles. Ordinary roles can only set defaults for themselves. Certain configuration variables cannot be set this way, or can only be set if a superuser issues the command. Only superusers can change a setting for all roles in all databases.

**RESOURCE QUEUE**
:   Assigns the role to a resource queue. The role would then be subject to the limits assigned to the resource queue when issuing queries. Specify `NONE` to assign the role to the default resource queue. A role can belong to only one resource queue. For a role without `LOGIN` privilege, resource queues have no effect. See [CREATE RESOURCE QUEUE](CREATE_RESOURCE_QUEUE.html) for more information.

**RESOURCE GROUP**
:   Assigns a resource group to the role. The role would then be subject to the concurrent transaction, memory, and CPU limits configured for the resource group. You can assign a single resource group to one or more roles. You cannot assign a resource group that you create for an external component to a role. See [CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html) for additional information.

## <a id="section4"></a>Parameters 

name
:   The name of the role whose attributes are to be altered.

CURRENT_USER
:   Alter the current user instead of an explicitly identified role.

SESSION_USER
:   Alter the current session user instead of an explicitly identified role.

SUPERUSER
NOSUPERUSER
CREATEDB
NOCREATEDB
CREATEROLE
NOCREATEROLE
INHERIT
NOINHERIT
LOGIN
NOLOGIN
REPLICATION
NOREPLICATION
BYPASSRLS
NOBYPASSRLS
CONNECTION LIMIT connlimit
[ ENCRYPTED ] PASSWORD 'password'
PASSWORD NULL
VALID UNTIL 'timestamp'
:   These clauses alter attributes originally set by `CREATE ROLE`. For more information, see the [CREATE ROLE](CREATE_ROLE.html) reference page.

new\_name
:   The new name of the role.

database\_name
:   The name of the database in which to set the configuration parameter.

configuration\_parameter
value
:   Set this role's session default for the specified configuration parameter to the given value. If value is `DEFAULT` or, equivalently, `RESET` is used, the role-specific parameter setting is removed, so the role will inherit the system-wide default setting in new sessions. Use `RESET ALL` to clear all role-specific settings. `SET FROM CURRENT` saves the session's current value of the parameter as the role-specific value. If `IN DATABASE` is specified, the configuration parameter is set or removed for the given role and database only.

:   Role-specific variable settings take effect only at login; [SET ROLE](SET_ROLE.html) and [SET SESSION AUTHORIZATION](SET_SESSION_AUTHORIZATION.html) do not process role-specific variable settings.

:   See [SET](SET.html) and [Server Configuration Parameters](../config_params/guc_config.html) for more information about allowed parameter names and values.

group\_name
:   The name of the resource group to assign to this role. Specifying the group\_name `NONE` removes the role's current resource group assignment and assigns a default resource group based on the role's capability. `SUPERUSER` roles are assigned the `admin_group` resource group, while the `default_group` resource group is assigned to non-admin roles.

:   You cannot assign a resource group that you create for an external component to a role.

queue\_name
:   The name of the resource queue to which the user-level role is to be assigned. Only roles with `LOGIN` privilege can be assigned to a resource queue. To unassign a role from a resource queue and put it in the default resource queue, specify `NONE`. A role can belong only to one resource queue.

## <a id="section5"></a>Notes 

Use [CREATE ROLE](CREATE_ROLE.html) to add a new role, and [DROP ROLE](DROP_ROLE.html) to remove a role.

`ALTER ROLE` cannot change a role's memberships; use [GRANT](GRANT.html) and [REVOKE](REVOKE.html) to do that.

You must exercise caution when specifying an unencrypted password with this command. The password will be transmitted to the server in clear text, and it might also be logged in the client's command history or the server log. The [psql](../../utility_guide/ref/psql.html) command-line client contains a meta-command `\password` that can be used to change a role's password without exposing the clear text password.

It is also possible to tie a session default to a specific database rather than to a role; see [ALTER DATABASE](ALTER_DATABASE.html). If there is a conflict, database-role-specific settings override role-specific ones, which in turn override database-specific ones.

## <a id="section6"></a>Examples 

Change the password for a role:

```
ALTER ROLE daria WITH PASSWORD 'passwd123';
```

Remove a role's password:

```
ALTER ROLE daria WITH PASSWORD NULL;
```

Change a password expiration date, specifying that the password should expire at midday on 4th May 2022 using the time zone which is one hour ahead of UTC:

```
ALTER ROLE scott VALID UNTIL 'May 4 12:00:00 2022 +1';
```

Make a password valid forever:

```
ALTER ROLE luke VALID UNTIL 'infinity';
```

Give a role the ability to create other roles and new databases:

```
ALTER ROLE joelle CREATEROLE CREATEDB;
```

Give a role a non-default setting of the `maintenance_work_mem` parameter:

```
ALTER ROLE admin SET maintenance_work_mem = 100000;
```

Give a role a non-default, database-specific setting of the `client_min_messages` parameter:

```
ALTER ROLE fred IN DATABASE devel SET client_min_messages = DEBUG;
```

Assign a role to a resource queue:

```
ALTER ROLE sammy RESOURCE QUEUE poweruser;
```

Give a role permission to create writable external tables:

```
ALTER ROLE load CREATEEXTTABLE (type='writable');
```

Alter a role so it does not allow login access on Sundays:

```
ALTER ROLE user3 DENY DAY 'Sunday';
```

Alter a role to remove the constraint that does not allow login access on Sundays:

```
ALTER ROLE user3 DROP DENY FOR DAY 'Sunday';
```

Assign a new resource group to a role:

```
ALTER ROLE parttime_user RESOURCE GROUP rg_light;
```

## <a id="section7"></a>Compatibility 

The `ALTER ROLE` statement is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE ROLE](CREATE_ROLE.html), [DROP ROLE](DROP_ROLE.html), [ALTER DATABASE](ALTER_DATABASE.html), [SET](SET.html), [CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html), [CREATE RESOURCE QUEUE](CREATE_RESOURCE_QUEUE.html), [GRANT](GRANT.html), [REVOKE](REVOKE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

