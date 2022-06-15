# DROP RESOURCE GROUP 

Removes a resource group.

## Synopsis 

``` {#sql_command_synopsis}
DROP RESOURCE GROUP <group_name>
```

## Description 

This command removes a resource group from Greenplum Database. Only a superuser can drop a resource group. When you drop a resource group, the memory and CPU resources reserved by the group are returned to Greenplum Database.

To drop a role resource group, the group cannot be assigned to any roles, nor can it have any statements pending or running in the group. If you drop a resource group that you created for an external component, the behavior is determined by the external component. For example, dropping a resource group that you assigned to a PL/Container runtime kills running containers in the group.

You cannot drop the pre-defined `admin_group` and `default_group` resource groups.

## Parameters 

group\_name
:   The name of the resource group to remove.

## Notes 

You cannot submit a `DROP RESOURCE GROUP` command in an explicit transaction or sub-transaction.

Use [ALTER ROLE](ALTER_ROLE.html) to remove a resource group assigned to a specific user/role.

Perform the following query to view all of the currently active queries for all resource groups:

```
SELECT usename, current_query, waiting, procpid,
    rsgid, rsgname, rsgqueueduration 
  FROM pg_stat_activity;

```

To view the resource group assignments, perform the following query on the `pg_roles` and `pg_resgroup` system catalog tables:

```
SELECT rolname, rsgname 
  FROM pg_roles, pg_resgroup
  WHERE pg_roles.rolresgroup=pg_resgroup.oid;
```

## Examples 

Remove the resource group assigned to a role. This operation then assigns the default resource group `default_group` to the role:

```
ALTER ROLE bob RESOURCE GROUP NONE;
```

Remove the resource group named `adhoc`:

```
DROP RESOURCE GROUP adhoc;
```

## Compatibility 

The `DROP RESOURCE GROUP` statement is a Greenplum Database extension.

## See Also 

[ALTER RESOURCE GROUP](ALTER_RESOURCE_GROUP.html), [CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

