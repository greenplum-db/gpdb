# ALTER USER MAPPING 

Changes the definition of a user mapping for a foreign server.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER USER MAPPING FOR { <user_name> | USER | CURRENT_USER | SESSION_USER | PUBLIC }
    SERVER <server_name>
    OPTIONS ( [ ADD | SET | DROP ] <option> ['<value>'] [, ... ] )
```

## <a id="section3"></a>Description 

`ALTER USER MAPPING` changes the definition of a user mapping for a foreign server.

The owner of a foreign server can alter user mappings for that server for any user. Also, a user granted `USAGE` privilege on the server can alter a user mapping for their own user name.

## <a id="section4"></a>Parameters 

user\_name
:   User name of the mapping. `CURRENT_USER` and `USER` match the name of the current user. `PUBLIC` is used to match all present and future user names in the system.

server\_name
:   Server name of the user mapping.

OPTIONS \( \[ ADD \| SET \| DROP \] option \['value'\] \[, ... \] \)
:   Change options for the user mapping. The new options override any previously specified options. `ADD`, `SET`, and `DROP` specify the action to perform. If no operation is explicitly specified, the default operation is `ADD`. Option names must be unique. Greenplum Database validates names and values using the server's foreign-data wrapper.

## <a id="section6"></a>Examples 

Change the password for user mapping `bob`, server `foo`:

```
ALTER USER MAPPING FOR bob SERVER foo OPTIONS (SET password 'public');
```

## <a id="section7"></a>Compatibility 

`ALTER USER MAPPING` conforms to ISO/IEC 9075-9 \(SQL/MED\). There is a subtle syntax issue: The standard omits the `FOR` key word. Since both `CREATE USER MAPPING` and `DROP USER MAPPING` use `FOR` in analogous positions, Greenplum Database diverges from the standard here in the interest of consistency and interoperability.

## <a id="section8"></a>See Also 

[CREATE USER MAPPING](CREATE_USER_MAPPING.html), [DROP USER MAPPING](DROP_USER_MAPPING.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

