# CREATE USER MAPPING 

Defines a new mapping of a user to a foreign server.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE USER MAPPING [ IF NOT EXISTS ] FOR { <user_name> | USER | CURRENT_USER | PUBLIC }
    SERVER <server_name>
    [ OPTIONS ( <option> '<value>' [, ... ] ) ]
```

## <a id="section3"></a>Description 

`CREATE USER MAPPING` defines a mapping of a user to a foreign server. A user mapping typically encapsulates connection information that a foreign-data wrapper uses together with the information encapsulated by a foreign server to access an external data resource.

The owner of a foreign server can create user mappings for that server for any user. Also, a user can create a user mapping for their own user name if they have been granted `USAGE` privilege on the server.

## <a id="section4"></a>Parameters 

IF NOT EXISTS
:   Do not throw an error if a mapping of the given user to the given foreign server already exists. Greenplum Database issues a notice in this case. Note that there is no guarantee that the existing user mapping is anything like the one that would have been created.

user\_name
:   The name of an existing user that is mapped to the foreign server. `CURRENT_USER` and `USER` match the name of the current user. When `PUBLIC` is specified, Greenplum Database creates a so-called public mapping that is used when no user-specific mapping is applicable.

server\_name
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

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

