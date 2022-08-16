# CREATE PROTOCOL 

Registers a custom data access protocol that can be specified when defining a Greenplum Database external table.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [TRUSTED] PROTOCOL <name> (
   [readfunc='<read_call_handler>'] [, writefunc='<write_call_handler>']
   [, validatorfunc='<validate_handler>' ])
```

## <a id="section3"></a>Description 

`CREATE PROTOCOL` associates a data access protocol name with call handlers that are responsible for reading from and writing data to an external data source. You must be a superuser to create a protocol.

The `CREATE PROTOCOL` command must specify either a read call handler or a write call handler. The call handlers specified in the `CREATE PROTOCOL` command must be defined in the database.

The protocol name can be specified in a `CREATE EXTERNAL TABLE` command.

For information about creating and enabling a custom data access protocol, see "Example Custom Data Access Protocol" in the *Greenplum Database Administrator Guide*.

## <a id="section4"></a>Parameters 

TRUSTED
:   If not specified, only superusers and the protocol owner can create external tables using the protocol. If specified, superusers and the protocol owner can `GRANT` permissions on the protocol to other database roles.

name
:   The name of the data access protocol. The protocol name is case sensitive. The name must be unique among the protocols in the database.

readfunc= 'read\_call\_handler'
:   The name of a previously registered function that Greenplum Database calls to read data from an external data source. The command must specify either a read call handler or a write call handler.

writefunc= 'write\_call\_handler'
:   The name of a previously registered function that Greenplum Database calls to write data to an external data source. The command must specify either a read call handler or a write call handler.

validatorfunc='validate\_handler'
:   An optional validator function that validates the URL specified in the `CREATE EXTERNAL TABLE` command.

## <a id="section5"></a>Notes 

Greenplum Database handles external tables of type `file`, `gpfdist`, and `gpfdists` internally. See [s3:// Protocol](../../admin_guide/external/g-s3-protocol.html#amazon-emr/s3_prereq) for information about enabling the `S3` protocol. Refer to [pxf:// Protocol](../../admin_guide/external/g-pxf-protocol.html) for information about using the `pxf` protocol.

Any shared library that implements a data access protocol must be located in the same location on all Greenplum Database segment hosts. For example, the shared library can be in a location specified by the operating system environment variable `LD_LIBRARY_PATH` on all hosts. You can also specify the location when you define the handler function. For example, when you define the `s3` protocol in the `CREATE PROTOCOL` command, you specify `$libdir/gps3ext.so` as the location of the shared object, where `$libdir` is located at `$GPHOME/lib`.

## <a id="section7"></a>Compatibility 

`CREATE PROTOCOL` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[ALTER PROTOCOL](ALTER_PROTOCOL.html), [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [DROP PROTOCOL](DROP_PROTOCOL.html), [GRANT](GRANT.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

