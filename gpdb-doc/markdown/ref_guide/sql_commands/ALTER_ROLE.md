# ALTER ROLE 

Changes a database role \(user or group\).

## Synopsis 

``` {#sql_command_synopsis}
ALTER ROLE <name> RENAME TO <newname>

ALTER ROLE <name> SET <config_parameter> {TO | =} {<value> | DEFAULT}

ALTER ROLE <name> RESET <config_parameter>

ALTER ROLE <name> RESOURCE QUEUE {<queue_name> | NONE}

ALTER ROLE <name> RESOURCE GROUP {<group_name> | NONE}

ALTER ROLE <name> [ [WITH] <option> [ ... ] ]
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

`ALTER ROLE` changes the attributes of a Greenplum Database role. There are several variants of this command:

-   **RENAME** — Changes the name of the role. Database superusers can rename any role. Roles having `CREATEROLE` privilege can rename non-superuser roles. The current session user cannot be renamed \(connect as a different user to rename a role\). Because MD5-encrypted passwords use the role name as cryptographic salt, renaming a role clears its password if the password is MD5-encrypted.
-   **SET \| RESET** — changes a role's session default for a specified configuration parameter. Whenever the role subsequently starts a new session, the specified value becomes the session default, overriding whatever setting is present in server configuration file \(`postgresql.conf`\). For a role without `LOGIN` privilege, session defaults have no effect. Ordinary roles can change their own session defaults. Superusers can change anyone's session defaults. Roles having `CREATEROLE` privilege can change defaults for non-superuser roles. See the *Greenplum Database Administrator Guide* for information about all user-settable configuration parameters.
-   **RESOURCE QUEUE** — Assigns the role to a resource queue. The role would then be subject to the limits assigned to the resource queue when issuing queries. Specify `NONE` to assign the role to the default resource queue. A role can only belong to one resource queue. For a role without `LOGIN` privilege, resource queues have no effect. See [CREATE RESOURCE QUEUE](CREATE_RESOURCE_QUEUE.html) for more information.
-   **RESOURCE GROUP** — Assigns a resource group to the role. The role would then be subject to the concurrent transaction, memory, and CPU limits configured for the resource group. You can assign a single resource group to one or more roles. You cannot assign a resource group that you create for an external component to a role. See [CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html) for additional information.
-   **WITH option** — Changes many of the role attributes that can be specified in [CREATE ROLE](CREATE_ROLE.html). Attributes not mentioned in the command retain their previous settings. Database superusers can change any of these settings for any role. Roles having `CREATEROLE` privilege can change any of these settings, but only for non-superuser roles. Ordinary roles can only change their own password.

## Parameters 

name
:   The name of the role whose attributes are to be altered.

newname
:   The new name of the role.

config\_parameter=value
:   Set this role's session default for the specified configuration parameter to the given value. If value is `DEFAULT` or if `RESET` is used, the role-specific variable setting is removed, so the role will inherit the system-wide default setting in new sessions. Use `RESET ALL` to clear all role-specific settings. See [SET](SET.html) and [Server Configuration Parameters](../config_params/guc_config.html) for information about user-settable configuration parameters.

group\_name
:   The name of the resource group to assign to this role. Specifying the group\_name `NONE` removes the role's current resource group assignment and assigns a default resource group based on the role's capability. `SUPERUSER` roles are assigned the `admin_group` resource group, while the `default_group` resource group is assigned to non-admin roles.

:   You cannot assign a resource group that you create for an external component to a role.

queue\_name
:   The name of the resource queue to which the user-level role is to be assigned. Only roles with `LOGIN` privilege can be assigned to a resource queue. To unassign a role from a resource queue and put it in the default resource queue, specify `NONE`. A role can only belong to one resource queue.

SUPERUSER \| NOSUPERUSER
CREATEDB \| NOCREATEDB
CREATEROLE \| NOCREATEROLE
CREATEUSER \| NOCREATEUSER
:   `CREATEUSER` and `NOCREATEUSER` are obsolete, but still accepted, spellings of `SUPERUSER` and `NOSUPERUSER`. Note that they are not equivalent to the `CREATEROLE and` `NOCREATEROLE` clauses.

CREATEEXTTABLE \| NOCREATEEXTTABLE \[\(attribute='value'\)\]
:   If `CREATEEXTTABLE` is specified, the role being defined is allowed to create external tables. The default `type` is `readable` and the default `protocol` is `gpfdist` if not specified. `NOCREATEEXTTABLE` \(the default\) denies the role the ability to create external tables. Note that external tables that use the `file` or `execute` protocols can only be created by superusers.

INHERIT \| NOINHERIT
LOGIN \| NOLOGIN
CONNECTION LIMIT connlimit
PASSWORD password
ENCRYPTED \| UNENCRYPTED
VALID UNTIL 'timestamp'
:   These clauses alter role attributes originally set by [CREATE ROLE](CREATE_ROLE.html).

DENY deny\_point
DENY BETWEEN deny\_point AND deny\_point
:   The `DENY` and `DENY BETWEEN` keywords set time-based constraints that are enforced at login. `DENY`sets a day or a day and time to deny access. `DENY BETWEEN` sets an interval during which access is denied. Both use the parameter deny\_point that has following format:

    ```
    DAY day [ TIME 'time' ]
    ```

:   The two parts of the `deny_point` parameter use the following formats:

    For day:

    ```
    {'Sunday' | 'Monday' | 'Tuesday' |'Wednesday' | 'Thursday' | 'Friday' | 
    'Saturday' | 0-6 }
    ```

    For `time:`

    \{ 00-23 : 00-59 \| 01-12 : 00-59 \{ AM \| PM \}\}

:   The `DENY BETWEEN` clause uses two deny\_point parameters.

:   ```
DENY BETWEEN <deny_point> AND <deny_point>
```

:   For more information about time-based constraints and examples, see "Managing Roles and Privileges" in the *Greenplum Database Administrator Guide*.

DROP DENY FOR deny\_point
:   The `DROP DENY FOR` clause removes a time-based constraint from the role. It uses the deny\_point parameter described above.

:   For more information about time-based constraints and examples, see "Managing Roles and Privileges" in the *Greenplum Database Administrator Guide*.

## Notes 

Use [GRANT](GRANT.html) and [REVOKE](REVOKE.html) for adding and removing role memberships.

Caution must be exercised when specifying an unencrypted password with this command. The password will be transmitted to the server in clear text, and it might also be logged in the client's command history or the server log. The `psql` command-line client contains a meta-command `\password` that can be used to safely change a role's password.

It is also possible to tie a session default to a specific database rather than to a role. Role-specific settings override database-specific ones if there is a conflict. See [ALTER DATABASE](ALTER_DATABASE.html).

## Examples 

Change the password for a role:

```
ALTER ROLE daria WITH PASSWORD 'passwd123';
```

Change a password expiration date:

```
ALTER ROLE scott VALID UNTIL 'May 4 12:00:00 2015 +1';
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

## Compatibility 

The `ALTER ROLE` statement is a Greenplum Database extension.

## See Also 

[CREATE ROLE](CREATE_ROLE.html), [DROP ROLE](DROP_ROLE.html), [SET](SET.html), [CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html), [CREATE RESOURCE QUEUE](CREATE_RESOURCE_QUEUE.html), [GRANT](GRANT.html), [REVOKE](REVOKE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

