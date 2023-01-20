# CREATE SERVER 

Defines a new foreign server.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE SERVER [ IF NOT EXISTS ] <server_name> [ TYPE '<server_type>' ] [ VERSION '<server_version>' ]
    FOREIGN DATA WRAPPER <fdw_name>
    [ OPTIONS ( [ mpp_execute { 'master' | 'any' | 'all segments' } [, ] ]
                [ num_segments '<num>' [, ] ]
                [ <option> '<value>' [, ... ]] ) ]
```

## <a id="section3"></a>Description 

`CREATE SERVER` defines a new foreign server. The user who defines the server becomes its owner.

A foreign server typically encapsulates connection information that a foreign-data wrapper uses to access an external data source. Additional user-specific connection information may be specified by means of user mappings.

Creating a server requires the `USAGE` privilege on the foreign-data wrapper specified.

## <a id="section4"></a>Parameters 

IF NOT EXISTS

:   Do not throw an error if a server with the same name already exists. Greenplum Database issues a notice in this case. Note that there is no guarantee that the existing server is anything like the one that would have been created.

server\_name
:   The name of the foreign server to create. The server name must be unique within the database.

server\_type
:   Optional server type, potentially useful to foreign-data wrappers.

server\_version
:   Optional server version, potentially useful to foreign-data wrappers.

fdw\_name
:   Name of the foreign-data wrapper that manages the server.

OPTIONS \( option 'value' \[, ... \] \)
:   The options for the new foreign server. The options typically define the connection details of the server, but the actual names and values are dependent upon the server's foreign-data wrapper.

mpp\_execute \{ 'master' \| 'any' \| 'all segments' \}
:   A Greenplum Database-specific option that identifies the host from which the foreign-data wrapper reads or writes data:

    -   `master` \(the default\)—Read or write data from the coordinator host.
    -   `any`—Read data from either the coordinator host or any one segment, depending on which path costs less.
    -   `all segments`—Read or write data from all segments. To support this option value, the foreign-data wrapper should have a policy that matches the segments to data.

    > **Note** Greenplum Database supports parallel writes to foreign tables only when you set `mpp_execute 'all segments'`.

    Support for the foreign server `mpp_execute` option, and the specific modes, is foreign-data wrapper-specific.

    The `mpp_execute` option can be specified in multiple commands: `CREATE FOREIGN TABLE`, `CREATE SERVER`, and `CREATE FOREIGN DATA WRAPPER`. The foreign table setting takes precedence over the foreign server setting, followed by the foreign-data wrapper setting.

num\_segments 'num'
:   When `mpp_execute` is set to `'all segments'`, the Greenplum Database-specific `num_segments` option identifies the number of query executors that Greenplum Database spawns on the source Greenplum Database cluster. If you do not provide a value, num defaults to the number of segments in the source cluster.

    Support for the foreign server `num_segments` option is foreign-data wrapper-specific.

## <a id="section5"></a>Notes 

When using the dblink module \(see [dblink](../modules/dblink.html)\), you can use the foreign server name as an argument of the `dblink_connect()` function to provide the connection parameters. You must have the `USAGE` privilege on the foreign server to use it in this manner.

## <a id="section6"></a>Examples 

Create a foreign server named `myserver` that uses a foreign-data wrapper named `gpfdw1` and includes connection options:

```
CREATE SERVER myserver FOREIGN DATA WRAPPER gpfdw1 
    OPTIONS (host 'foo', dbname 'foodb', port '5432');
```

## <a id="section7"></a>Compatibility 

`CREATE SERVER` conforms to ISO/IEC 9075-9 \(SQL/MED\).

## <a id="section8"></a>See Also 

[ALTER SERVER](ALTER_SERVER.html), [DROP SERVER](DROP_SERVER.html), [CREATE FOREIGN DATA WRAPPER](CREATE_FOREIGN_DATA_WRAPPER.html), [CREATE FOREIGN TABLE](CREATE_FOREIGN_TABLE.html), [CREATE USER MAPPING](CREATE_USER_MAPPING.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

