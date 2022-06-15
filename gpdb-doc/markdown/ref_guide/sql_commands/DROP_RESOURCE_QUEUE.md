# DROP RESOURCE QUEUE 

Removes a resource queue.

## Synopsis 

``` {#sql_command_synopsis}
DROP RESOURCE QUEUE <queue_name>
```

## Description 

This command removes a resource queue from Greenplum Database. To drop a resource queue, the queue cannot have any roles assigned to it, nor can it have any statements waiting in the queue. Only a superuser can drop a resource queue.

## Parameters 

queue\_name
:   The name of a resource queue to remove.

## Notes 

Use [ALTER ROLE](ALTER_ROLE.html) to remove a user from a resource queue.

To see all the currently active queries for all resource queues, perform the following query of the `pg_locks` table joined with the `pg_roles` and `pg_resqueue` tables:

```
SELECT rolname, rsqname, locktype, objid, pid, 
mode, granted FROM pg_roles, pg_resqueue, pg_locks WHERE 
pg_roles.rolresqueue=pg_locks.objid AND 
pg_locks.objid=pg_resqueue.oid;
```

To see the roles assigned to a resource queue, perform the following query of the `pg_roles` and `pg_resqueue` system catalog tables:

```
SELECT rolname, rsqname FROM pg_roles, pg_resqueue WHERE 
pg_roles.rolresqueue=pg_resqueue.oid;
```

## Examples 

Remove a role from a resource queue \(and move the role to the default resource queue, `pg_default`\):

```
ALTER ROLE bob RESOURCE QUEUE NONE;
```

Remove the resource queue named `adhoc`:

```
DROP RESOURCE QUEUE adhoc;
```

## Compatibility 

The `DROP RESOURCE QUEUE` statement is a Greenplum Database extension.

## See Also 

[ALTER RESOURCE QUEUE](ALTER_RESOURCE_QUEUE.html), [CREATE RESOURCE QUEUE](CREATE_RESOURCE_QUEUE.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

