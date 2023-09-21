---
title: About Changes to External Tables in Greenplum 7
---

This topic describes the external table implementation and changes in Greenplum 7, and is geared towards existing users of Greenplum 6. Greenplum 7 converts an external table that you define with the [CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html) command into a foreign table, and internally operates on and represents the table using the foreign table data structures and catalog.

(See also [Understanding the External Table to Foreign Table Mapping](map_ext_to_foreign.html) for detailed information about the external table to foreign table conversion, and its runtime implications.)

**Parent topic:** [Accessing External Data with External Tables](../external/g-external-tables.html)

## <a id="not"></a>What's the Same?

If you used external tables in Greenplum 6, the underlying functionality has not changed in Greenplum 7. The following external table features and behaviors remain the same in VMware Greenplum 7:

- Greenplum 7 fully supports the external table SQL command syntax of Greenplum 6.
- Greenplum 7 fully supports external table access to remote data sources via all existing protocols (`file`, `gpdist`, `pxf`, and `s3`).
- You must create separate tables to read from (`CREATE EXTERNAL TABLE`) and write to (`CREATE WRITABLE EXTERNAL TABLE`) the same external data location.
- The [pg_exttable](../../ref_guide/system_catalogs/catalog_ref-views.html#pg_exttable) system catalog (now a view) provides the same information.

## <a id="changed"></a>What Has Changed?

Note the following differences in the Greenplum 7 external table implementation compared to Greenplum 6:

- Greenplum 7 uses foreign table data structures and catalogs to internally represent external tables. Use the [pg_foreign_table](../../ref_guide/system_catalogs/pg_foreign_table.html) system catalog table and the `ftoptions` column to view the table definition.
- A `pg_tables` query no longer returns external tables in the query results.
- The `pg_class.relkind` of an external table is now `f` (was previously `r`).
- The [pg_exttable](../../ref_guide/system_catalogs/catalog_ref-views.html#pg_exttable) system catalog is now a view.
- In addition to `pg_exttable`, you can use the following query to list all of the foreign tables that were created using the `CREATE [WRITABLE] EXTERNAL TABLE` command:

    ```
    SELECT * FROM pg_foreign_table ft 
      JOIN pg_foreign_server fs ON ft.ftserver = fs.oid
      WHERE srvname = 'gp_exttable_server';
    ```
- Because an external table is internally represented as a foreign table:

    - Every external table is associated with the `gp_exttable_fdw` foreign-data wrapper.
    - Every external table is associated with the `gp_exttable_server` foreign server.
    - Certain command output and error, detail, and notice messages about external tables refer to the table as a foreign table.
    - External tables are included in the foreign table catalogs, for example [pg_foreign_table](../../ref_guide/system_catalogs/pg_foreign_table.html).
    - External tables are included when you list or examine foreign tables (for example, the `\det` `psql` meta-command).
- External table-specific information displayed in `psql` `\dE+` output has changed; the relation `Type` of an external table is now `foreign table`. Example:

    ```
    \dE
                    List of relations
     Schema |     Name     |     Type      |  Owner  
    --------+--------------+---------------+---------
     public | ext_expenses | foreign table | gpadmin
    ```

- External table-specific information displayed in `psql` `\d+ <external_table_name>` output has changed; it now displays in foreign table format. For this example `CREATE EXTERNAL TABLE` call:

    ```
    CREATE EXTERNAL TABLE ext_expenses ( name text, date date, amount float4, category text, desc1 varchar )
      LOCATION ('gpfdist://etlhost-1:8081/*.txt', 'gpfdist://etlhost-2:8082/*.txt')
    FORMAT 'TEXT' ( DELIMITER '|' NULL ' ' )
    LOG ERRORS SEGMENT REJECT LIMIT 5;
    ```

    The example `\d+` output follows:

    ```
    \d+ ext_expenses
                                             Foreign table "public.ext_expenses"
      Column  |       Type        | Collation | Nullable | Default | FDW options | Storage  | Stats target | Description 
    ----------+-------------------+-----------+----------+---------+-------------+----------+--------------+-------------
     name     | text              |           |          |         |             | extended |              | 
     date     | date              |           |          |         |             | plain    |              | 
     amount   | real              |           |          |         |             | plain    |              | 
     category | text              |           |          |         |             | extended |              | 
     desc1    | character varying |           |          |         |             | extended |              | 
    FDW options: (format 'text', delimiter '|', "null" ' ', escape E'\\', location_uris 'gpfdist://etlhost-1:8081/\*.txt|'gpfdist://etlhost-2:8082/\*.txt', execute_on 'ALL_SEGMENTS', reject_limit '5', reject_limit_type 'rows', log_errors 'enable', encoding 'UTF8', is_writable 'false')
    ```
- The `EXPLAIN` output for a query including an external table previously returned the text `External Scan`. `EXPLAIN` now returns `Foreign Scan` in this scenario.

## <a id="other"></a>Additional Considerations

Additional factors to consider:

- Even though an external table is internally represented as a foreign table, you cannot both read from and write to the same external table.
- You must change any scripts that you wrote that depend on external table DDL or `psql` `\dE` or `\d+` output.
- Greenplum 7 dumps and restores the DDL of external tables using foreign table syntax.

