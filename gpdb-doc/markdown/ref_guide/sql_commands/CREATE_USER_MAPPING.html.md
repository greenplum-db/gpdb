---
title: CREATE USER MAPPING 
---

Defines a new mapping of a user to a foreign server.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE USER MAPPING FOR { <username> | USER | CURRENT_USER | PUBLIC }
    SERVER <servername>
    [ OPTIONS ( <option> '<value>' [, ... ] ) ]
```

## <a id="section3"></a>Description 

`CREATE USER MAPPING` defines a mapping of a user to a foreign server. You must be the owner of the server to define user mappings for it.

## <a id="section4"></a>Parameters 

username
:   The name of an existing user that is mapped to the foreign server. `CURRENT_USER` and `USER` match the name of the current user. `PUBLIC` is used to match all present and future user names in the system.

servername
:   The name of an existing server for which Greenplum Database is to create the user mapping.

OPTIONS \( option 'value' \[, ... \] \)
:   The options for the new user mapping. The options typically define the actual user name and password of the mapping. Option names must be unique. The option names and values are specific to the server's foreign-data wrapper.

## <a id="section6"></a>Examples 

Create a user mapping for user `bob`, server `foo`:

```
CREATE USER MAPPING FOR bob SERVER foo OPTIONS (user 'bob', password 'secret');
```

## <a id="section7"></a>Compatibility 

`CREATE USER MAPPING` conforms to ISO/IEC 9075-9 \(SQL/MED\).

## <a id="section8"></a>See Also 

[ALTER USER MAPPING](ALTER_USER_MAPPING.html), [DROP USER MAPPING](DROP_USER_MAPPING.html), [CREATE FOREIGN DATA WRAPPER](CREATE_FOREIGN_DATA_WRAPPER.html), [CREATE SERVER](CREATE_SERVER.html)

**Parent topic:**[SQL Commands](../sql_commands/sql_ref.html)

