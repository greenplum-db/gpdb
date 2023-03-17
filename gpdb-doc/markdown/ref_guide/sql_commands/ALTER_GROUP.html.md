# ALTER GROUP 

Changes a role name or membership.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER GROUP <role_specification> ADD USER <user_name> [, ... ]

ALTER GROUP <role_specification> DROP USER <user_name> [, ... ]

where <role_specification> can be:

    <role_name>
  | CURRENT_USER
  | SESSION_USER

ALTER GROUP <group_name> RENAME TO <new_name>
```

## <a id="section3"></a>Description 

`ALTER GROUP` changes the attributes of a user group. This is an obsolete command, though still accepted for backwards compatibility, because users and groups are superseded by the more general concept of roles.

The first two variants add users to a group or remove them from a group. \(Any role can play the part of either a "user" or "group" for this purpose. These variants are effectively equivalent to granting or revoking membership in the role named as the "group"; so, the preferred way to do this is to use [GRANT](GRANT.html) or [REVOKE](REVOKE.html).

The third variant changes the name of the group. This is exactly equivalent to renaming the role with [ALTER ROLE](ALTER_ROLE.html).

## <a id="section4"></a>Parameters 

group\_name
:   The name of the group \(role\) to modify.

user\_name
:   Users \(roles\) that are to be added to or removed from the group. The users \(roles\) must already exist; `ALTER GROUP` does not create or drop users.

new\_name
:   The new name of the group.

## <a id="section5"></a>Examples 

To add users to a group:

```
ALTER GROUP staff ADD USER karl, john;
```

To remove a user from a group:

```
ALTER GROUP workers DROP USER beth;
```

## <a id="section6"></a>Compatibility 

There is no `ALTER GROUP` statement in the SQL standard.

## <a id="section7"></a>See Also 

[ALTER ROLE](ALTER_ROLE.html), [GRANT](GRANT.html), [REVOKE](REVOKE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

