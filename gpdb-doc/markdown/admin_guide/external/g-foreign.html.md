---
title: Accessing External Data with Foreign Tables 
---

Greenplum Database implements portions of the SQL/MED specification, allowing you to access data that resides outside of Greenplum using regular SQL queries. Such data is referred to as *foreign* or external data.

You can access foreign data with help from a *foreign-data wrapper*. A foreign-data wrapper is a library that communicates with a remote data source. This library hides the source-specific connection and data access details.

The Greenplum Database distribution includes the [postgres\_fdw](../../ref_guide/modules/postgres_fdw.html) foreign data wrapper.

If none of the existing PostgreSQL or Greenplum Database foreign-data wrappers suit your needs, you can write your own as described in [Writing a Foreign Data Wrapper](g-devel-fdw.html).

To access foreign data, you create a *foreign server* object, which defines how to connect to a particular remote data source according to the set of options used by its supporting foreign-data wrapper. Then you create one or more *foreign tables*, which define the structure of the remote data. A foreign table can be used in queries just like a normal table, but a foreign table has no storage in the Greenplum Database server. Whenever a foreign table is accessed, Greenplum Database asks the foreign-data wrapper to fetch data from, or update data in \(if supported by the wrapper\), the remote source.

**Note:** GPORCA does not support foreign tables, a query on a foreign table always falls back to the Postgres Planner.

Accessing remote data may require authenticating to the remote data source. This information can be provided by a *user mapping*, which can provide additional data such as a user name and password based on the current Greenplum Database role.

For additional information, refer to the [CREATE FOREIGN DATA WRAPPER](../../ref_guide/sql_commands/CREATE_FOREIGN_DATA_WRAPPER.html), [CREATE SERVER](../../ref_guide/sql_commands/CREATE_SERVER.html), [CREATE USER MAPPING](../../ref_guide/sql_commands/CREATE_USER_MAPPING.html), and [CREATE FOREIGN TABLE](../../ref_guide/sql_commands/CREATE_FOREIGN_TABLE.html) SQL reference pages.

## <a id="greenplum"></a>Using Foreign-Data Wrappers with Greenplum Database 

Most PostgreSQL foreign-data wrappers should work with Greenplum Database. However, PostgreSQL foreign-data wrappers connect only through the Greenplum Database master by default and do not access the Greenplum Database segment instances directly.

Greenplum Database adds an `mpp_execute` option to FDW-related SQL commands. If the foreign-data wrapper supports it, you can specify `mpp_execute '*value*'` in the `OPTIONS` clause when you create the FDW, server, or foreign table to identify the Greenplum host from which the foreign-data wrapper reads or writes data. Valid `*value*`s are:

-   `master` \(the default\)—Read or write data from the master host.
-   `any`—Read data from either the master host or any one segment, depending on which path costs less.
-   `all segments`—Read or write data from all segments. If a foreign-data wrapper supports this value, for correct results it should have a policy that matches segments to data.

\(A PostgreSQL foreign-data wrapper may work with the various `mpp_execute` option settings, but the results are not guaranteed to be correct. For example, a segment may not be able to connect to the foriegn server, or segments may receive overlapping results resulting in duplicate rows.\)

**Note:** GPORCA does not support foreign tables, a query on a foreign table always falls back to the Postgres Planner.

**Parent topic:**[Working with External Data](../external/g-working-with-file-based-ext-tables.html)
