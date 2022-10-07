# IMPORT FOREIGN SCHEMA 

Imports table definitions from a foreign server.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
IMPORT FOREIGN SCHEMA <remote_schema>
    [ { LIMIT TO | EXCEPT } ( <table_name> [, ...] ) ]
    FROM SERVER <server_name>
    INTO <local_schema>
    [ OPTIONS ( <option> '<value>' [, ... ] ) ]
```

## <a id="section3"></a>Description 

`IMPORT FOREIGN SCHEMA` creates foreign tables that represent tables existing on a foreign server. The new foreign tables will be owned by the user issuing the command and are created with the correct column definitions and options to match the remote tables.

By default, all tables and views existing in a particular schema on the foreign server are imported. Optionally, the list of tables can be limited to a specified subset, or specific tables can be excluded. The new foreign tables are all created in the target schema, which must already exist.

To use `IMPORT FOREIGN SCHEMA`, the user must have `USAGE` privilege on the foreign server, as well as `CREATE` privilege on the target schema.

Support for importing foreign schemas is foreign-data wrapper-specific.

## <a id="section4"></a>Parameters 

remote\_schema
:   The remote schema to import from. The specific meaning of a remote schema depends on the foreign data wrapper in use.

LIMIT TO \( table\_name [, ...] \)
:   Import only foreign tables matching one of the given table names. Other tables existing in the foreign schema will be ignored.

EXCEPT \( table\_name [, ...] \)
:   Exclude specified foreign tables from the import. All tables existing in the foreign schema will be imported except the ones listed here.

server\_name
:   The name of the foreign server from which to import the table definitions.

local\_schema
:   The schema in which Greenplum Database will create the imported foreign tables.

OPTIONS \( option 'value' \[, ... \] \)
:   The options to be used during the import. The allowed option names and values are specific to each foreign-data wrapper.


## <a id="section6"></a>Examples 

Import table definitions from a remote schema `foreign_films` on server `film_server`, creating the foreign tables in local schema `films`:

```
IMPORT FOREIGN SCHEMA foreign_films
    FROM SERVER film_server INTO films;
```

As above, but import only the two tables `actors` and `directors` (if they exist):

```
IMPORT FOREIGN SCHEMA foreign_films LIMIT TO (actors, directors)
    FROM SERVER film_server INTO films;
```

## <a id="section7"></a>Compatibility 

The `IMPORT FOREIGN SCHEMA` command conforms to the SQL standard, except that the `OPTIONS` clause is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[CREATE FOREIGN TABLE](CREATE_FOREIGN_TABLE.html), [CREATE SERVER](CREATE_SERVER.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

