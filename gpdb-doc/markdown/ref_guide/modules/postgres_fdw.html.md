# postgres\_fdw 

The `postgres_fdw` module is a foreign data wrapper \(FDW\) that you can use to access data stored in a remote PostgreSQL or Greenplum database.

The Greenplum Database `postgres_fdw` module is a modified version of the PostgreSQL `postgres_fdw` module. The module behaves as described in the PostgreSQL [postgres\_fdw](https://www.postgresql.org/docs/12/postgres-fdw.html) documentation when you use it to access a remote PostgreSQL database.

> **Note** There are some restrictions and limitations when you use this foreign data wrapper module to access Greenplum Database, described below.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `postgres_fdw` module is installed when you install Greenplum Database. Before you can use the foreign data wrapper, you must register the `postgres_fdw` extension in each database in which you want to use the foreign data wrapper. Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_gp_limit"></a>Greenplum Database Limitations 

When you use the foreign data wrapper to access Greenplum Database, `postgres_fdw` has the following limitations:

-   The `ctid` is not guaranteed to uniquely identify the physical location of a row within its table. For example, the following statements may return incorrect results when the foreign table references a Greenplum Database table:

    ```
    INSERT INTO rem1(f2) VALUES ('test') RETURNING ctid;
    SELECT * FROM ft1, t1 WHERE t1.ctid = '(0,2)'; 
    ```

-   `postgres_fdw` does not support local or remote triggers when you use it to access a foreign table that references a Greenplum Database table.
-   `UPDATE` or `DELETE` operations on a foreign table that references a Greenplum table are not guaranteed to work correctly.

## <a id="topic_info"></a>Additional Module Documentation 

Refer to the [postgres\_fdw](https://www.postgresql.org/docs/12/postgres-fdw.html) PostgreSQL documentation for detailed information about this module.

