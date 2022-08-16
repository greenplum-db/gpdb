# DROP ROLE 

Removes a database role.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP ROLE [IF EXISTS] <name> [, ...]
```

## <a id="section3"></a>Description 

`DROP ROLE` removes the specified role\(s\). To drop a superuser role, you must be a superuser yourself. To drop non-superuser roles, you must have `CREATEROLE` privilege.

A role cannot be removed if it is still referenced in any database; an error will be raised if so. Before dropping the role, you must drop all the objects it owns \(or reassign their ownership\) and revoke any privileges the role has been granted on other objects. The [REASSIGN OWNED](REASSIGN_OWNED.html) and [DROP OWNED](DROP_OWNED.html) commands can be useful for this purpose.

However, it is not necessary to remove role memberships involving the role; `DROP ROLE` automatically revokes any memberships of the target role in other roles, and of other roles in the target role. The other roles are not dropped nor otherwise affected.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the role does not exist. A notice is issued in this case.

name
:   The name of the role to remove.

## <a id="section5"></a>Examples 

Remove the roles named `sally` and `bob`:

```
DROP ROLE sally, bob;
```

## <a id="section6"></a>Compatibility 

The SQL standard defines `DROP ROLE`, but it allows only one role to be dropped at a time, and it specifies different privilege requirements than Greenplum Database uses.

## <a id="section7"></a>See Also 

[REASSIGN OWNED](REASSIGN_OWNED.html), [DROP OWNED](DROP_OWNED.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER ROLE](ALTER_ROLE.html), [SET ROLE](SET_ROLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

