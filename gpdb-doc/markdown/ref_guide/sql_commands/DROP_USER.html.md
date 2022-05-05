---
title: DROP USER 
---

Removes a database role.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP USER [IF EXISTS] <name> [, ...]
```

## <a id="section3"></a>Description 

`DROP USER` is an alias for [DROP ROLE](DROP_ROLE.html). See [DROP ROLE](DROP_ROLE.html) for more information.

## <a id="section5"></a>Compatibility 

There is no `DROP USER` statement in the SQL standard. The SQL standard leaves the definition of users to the implementation.

## <a id="section6"></a>See Also 

[DROP ROLE](DROP_ROLE.html), [CREATE USER](CREATE_USER.html)

**Parent topic: **[SQL Commands](../sql_commands/sql_ref.html)

