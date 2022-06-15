# DROP OWNED 

Removes database objects owned by a database role.

## Synopsis 

``` {#sql_command_synopsis}
DROP OWNED BY <name> [, ...] [CASCADE | RESTRICT]
```

## Description 

`DROP OWNED` drops all the objects in the current database that are owned by one of the specified roles. Any privileges granted to the given roles on objects in the current database will also be revoked.

## Parameters 

name
:   The name of a role whose objects will be dropped, and whose privileges will be revoked.

CASCADE
:   Automatically drop objects that depend on the affected objects.

RESTRICT
:   Refuse to drop the objects owned by a role if any other database objects depend on one of the affected objects. This is the default.

## Notes 

`DROP OWNED` is often used to prepare for the removal of one or more roles. Because `DROP OWNED` only affects the objects in the current database, it is usually necessary to execute this command in each database that contains objects owned by a role that is to be removed.

Using the `CASCADE` option may make the command recurse to objects owned by other users.

The `REASSIGN OWNED` command is an alternative that reassigns the ownership of all the database objects owned by one or more roles.

## Examples 

Remove any database objects owned by the role named `sally`:

```
DROP OWNED BY sally;
```

## Compatibility 

The `DROP OWNED` statement is a Greenplum Database extension.

## See Also 

[REASSIGN OWNED](REASSIGN_OWNED.html), [DROP ROLE](DROP_ROLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

