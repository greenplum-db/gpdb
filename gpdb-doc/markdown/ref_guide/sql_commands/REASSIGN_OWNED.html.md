# REASSIGN OWNED 

Changes the ownership of database objects owned by a database role.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
REASSIGN OWNED BY { <old_role> | CURRENT_USER | SESSION_USER } [, ...]
               TO { <new_role> | CURRENT_USER | SESSION_USER }
```

## <a id="section3"></a>Description 

`REASSIGN OWNED` changes the ownership of database objects owned by any of the old\_roles to new\_role.

## <a id="section4"></a>Parameters 

old\_role
:   The name of a role. The ownership of all the objects in the current database, and of all shared objects \(databases, tablespaces\), owned by this role will be reassigned to new\_role.

new\_role
:   The name of the role that will be made the new owner of the affected objects.

## <a id="section5"></a>Notes 

`REASSIGN OWNED` is often used to prepare for the removal of one or more roles. Because `REASSIGN OWNED` does not affect objects in other databases, it is usually necessary to run this command in each database that contains objects owned by a role that is to be removed.

`REASSIGN OWNED` requires privileges on both the source role\(s\) and the target role.

The [DROP OWNED](DROP_OWNED.html) command is an alternative that simply drops all of the database objects owned by one or more roles.

The `REASSIGN OWNED` command does not affect any privileges granted to the old\_roles on objects that are not owned by them. Likewise, it does not affect default privileges created with `ALTER DEFAULT PRIVILEGES`. Use `DROP OWNED` to revoke such privileges.

## <a id="section6"></a>Examples 

Reassign any database objects owned by the role named `sally` and `bob` to `admin`:

```
REASSIGN OWNED BY sally, bob TO admin;
```

## <a id="section7"></a>Compatibility 

The `REASSIGN OWNED` command is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[DROP OWNED](DROP_OWNED.html), [DROP ROLE](DROP_ROLE.html), [ALTER DATABASE](ALTER_DATABASE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

