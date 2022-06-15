# REASSIGN OWNED 

Changes the ownership of database objects owned by a database role.

## Synopsis 

``` {#sql_command_synopsis}
REASSIGN OWNED BY <old_role> [, ...] TO <new_role>
```

## Description 

`REASSIGN OWNED` reassigns all the objects in the current database that are owned by old\_role to new\_role. Note that it does not change the ownership of the database itself.

## Parameters 

old\_role
:   The name of a role. The ownership of all the objects in the current database owned by this role will be reassigned to new\_role.

new\_role
:   The name of the role that will be made the new owner of the affected objects.

## Notes 

`REASSIGN OWNED` is often used to prepare for the removal of one or more roles. Because `REASSIGN OWNED` only affects the objects in the current database, it is usually necessary to execute this command in each database that contains objects owned by a role that is to be removed.

The `DROP OWNED` command is an alternative that drops all the database objects owned by one or more roles.

The `REASSIGN OWNED` command does not affect the privileges granted to the old roles in objects that are not owned by them. Use `DROP OWNED` to revoke those privileges.

## Examples 

Reassign any database objects owned by the role named `sally` and `bob` to `admin`;

```
REASSIGN OWNED BY sally, bob TO admin;
```

## Compatibility 

The `REASSIGN OWNED` statement is a Greenplum Database extension.

## See Also 

[DROP OWNED](DROP_OWNED.html), [DROP ROLE](DROP_ROLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

