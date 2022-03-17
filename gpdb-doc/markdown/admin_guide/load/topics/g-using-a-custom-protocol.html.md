---
title: Using a Custom Protocol 
---

Greenplum Database provides protocols such as gpfdist, `http`, and `file` for accessing data over a network, or you can author a custom protocol. You can use the standard data formats, `TEXT` and `CSV`, or a custom data format with custom protocols.

You can create a custom protocol whenever the available built-in protocols do not suffice for a particular need. For example, you could connect Greenplum Database in parallel to another system directly, and stream data from one to the other without the need to materialize the data on disk or use an intermediate process such as gpfdist. You must be a superuser to create and register a custom protocol.

1.  Author the send, receive, and \(optionally\) validator functions in C, with a predefined API. These functions are compiled and registered with the Greenplum Database. For an example custom protocol, see [Example Custom Data Access Protocol](g-example-custom-data-access-protocol.html).
2.  After writing and compiling the read and write functions into a shared object \(.so\), declare a database function that points to the .so file and function names.

    The following examples use the compiled import and export code.

    ```
    CREATE FUNCTION myread() RETURNS integer
    as '$libdir/gpextprotocol.so', 'myprot_import'
    LANGUAGE C STABLE;
    CREATE FUNCTION mywrite() RETURNS integer
    as '$libdir/gpextprotocol.so', 'myprot_export'
    LANGUAGE C STABLE;
    
    ```

    The format of the optional validator function is:

    ```
    CREATE OR REPLACE FUNCTION myvalidate() RETURNS void 
    AS '$libdir/gpextprotocol.so', 'myprot_validate' 
    LANGUAGE C STABLE; 
    
    ```

3.  Create a protocol that accesses these functions. `Validatorfunc` is optional.

    ```
    CREATE TRUSTED PROTOCOL myprot(
    writefunc='mywrite',
    readfunc='myread', 
    validatorfunc='myvalidate');
    ```

4.  Grant access to any other users, as necessary.

    ```
    GRANT ALL ON PROTOCOL myprot TO otheruser;
    
    ```

5.  Use the protocol in readable or writable external tables.

    ```
    CREATE WRITABLE EXTERNAL TABLE ext_sales(LIKE sales)
    LOCATION ('myprot://<meta>/<meta>/…')
    FORMAT 'TEXT';
    CREATE READABLE EXTERNAL TABLE ext_sales(LIKE sales)
    LOCATION('myprot://<meta>/<meta>/…')
    FORMAT 'TEXT';
    
    ```


Declare custom protocols with the SQL command `CREATE TRUSTED PROTOCOL`, then use the `GRANT` command to grant access to your users. For example:

-   Allow a user to create a readable external table with a trusted protocol

    ```
    GRANT SELECT ON PROTOCOL <protocol name> TO <user name>;
    ```

-   Allow a user to create a writable external table with a trusted protocol

    ```
    GRANT INSERT ON PROTOCOL <protocol name> TO <user name>;
    ```

-   Allow a user to create readable and writable external tables with a trusted protocol

    ```
    GRANT ALL ON PROTOCOL <protocol name> TO <user name>;
    ```


**Parent topic:**[Loading and Writing Non-HDFS Custom Data](../../load/topics/g-loading-and-writing-non-hdfs-custom-data.html)

