---
title: Accessing External Data with Foreign Tables 
---

Greenplum Database implements portions of the SQL/MED specification, allowing you to access data that resides outside of Greenplum using regular SQL queries. Such data is referred to as *foreign* or external data.

You can access foreign data with help from a *foreign-data wrapper*. A foreign-data wrapper is a library that communicates with a remote data source. This library hides the source-specific connection and data access details.

The Greenplum Database distribution includes the [postgres\_fdw](../../ref_guide/modules/postgres_fdw.html) foreign data wrapper.

**Note:** Most PostgreSQL foreign-data wrappers should work with Greenplum Database. However, PostgreSQL foreign-data wrappers connect only through the Greenplum Database master and do not access the Greenplum Database segment instances directly.

If none of the existing foreign-data wrappers suit your needs, you can write your own as described in [Writing a Foreign Data Wrapper](g-devel-fdw.html).

To access foreign data, you create a *foreign server* object, which defines how to connect to a particular remote data source according to the set of options used by its supporting foreign-data wrapper. Then you create one or more *foreign tables*, which define the structure of the remote data. A foreign table can be used in queries just like a normal table, but a foreign table has no storage in the Greenplum Database server. Whenever a foreign table is accessed, Greenplum Database asks the foreign-data wrapper to fetch data from, or update data in \(if supported by the wrapper\), the remote source.

**Note:** GPORCA does not support foreign tables, a query on a foreign table always falls back to the Postgres Planner.

Accessing remote data may require authenticating to the remote data source. This information can be provided by a *user mapping*, which can provide additional data such as a user name and password based on the current Greenplum Database role.

For additional information, refer to the [CREATE FOREIGN DATA WRAPPER](../../ref_guide/sql_commands/CREATE_FOREIGN_DATA_WRAPPER.html), [CREATE SERVER](../../ref_guide/sql_commands/CREATE_SERVER.html), [CREATE USER MAPPING](../../ref_guide/sql_commands/CREATE_USER_MAPPING.html), and [CREATE FOREIGN TABLE](../../ref_guide/sql_commands/CREATE_FOREIGN_TABLE.html) SQL reference pages.

-   **[Writing a Foreign Data Wrapper](../external/g-devel-fdw.html)**  


**Parent topic:**[Working with External Data](../external/g-working-with-file-based-ext-tables.html)

