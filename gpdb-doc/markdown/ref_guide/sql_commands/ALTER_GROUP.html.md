---
title: ALTER GROUP 
---

Changes a role name or membership.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER GROUP <groupname> ADD USER <username> [, ... ]

ALTER GROUP <groupname> DROP USER <username> [, ... ]

ALTER GROUP <groupname> RENAME TO <newname>
```

## <a id="section3"></a>Description 

`ALTER GROUP` changes the attributes of a user group. This is an obsolete command, though still accepted for backwards compatibility, because users and groups are superseded by the more general concept of roles. See [ALTER ROLE](ALTER_ROLE.html) for more information.

The first two variants add users to a group or remove them from a group. Any role can play the part of groupname or username. The preferred method for accomplishing these tasks is to use [GRANT](GRANT.html) and [REVOKE](REVOKE.html).

## <a id="section4"></a>Parameters 

groupname
:   The name of the group \(role\) to modify.

username
:   Users \(roles\) that are to be added to or removed from the group. The users \(roles\) must already exist.

newname
:   The new name of the group \(role\).

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

