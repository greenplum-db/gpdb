---
title: DROP USER MAPPING 
---

Removes a user mapping for a foreign server.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
DROP USER MAPPING [ IF EXISTS ] { <username> | USER | CURRENT_USER | PUBLIC } 
    SERVER <servername>
```

## <a id="section3"></a>Description 

`DROP USER MAPPING` removes an existing user mapping from a foreign server. To run this command, the current user must be the owner of the server containing the mapping.

## <a id="section4"></a>Parameters 

IF EXISTS
:   Do not throw an error if the user mapping does not exist. Greenplum Database issues a notice in this case.

username
:   User name of the mapping. `CURRENT_USER` and `USER` match the name of the current user. `PUBLIC` is used to match all present and future user names in the system.

servername
:   Server name of the user mapping.

## <a id="section6"></a>Examples 

Drop the user mapping named `bob`, server `foo` if it exists:

```
DROP USER MAPPING IF EXISTS FOR bob SERVER foo;
```

## <a id="section7"></a>Compatibility 

`DROP SERVER` conforms to ISO/IEC 9075-9 \(SQL/MED\). The `IF EXISTS` clause is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE USER MAPPING](CREATE_USER_MAPPING.html), [ALTER USER MAPPING](ALTER_USER_MAPPING.html)

**Parent topic:**[SQL Commands](../sql_commands/sql_ref.html)

