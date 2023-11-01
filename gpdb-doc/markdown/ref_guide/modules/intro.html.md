---
title: Additional Supplied Modules 
---

This section describes additional modules available in the Greenplum Database installation. These modules may be PostgreSQL- or Greenplum-sourced.

`contrib` modules are typically packaged as extensions. You register a module in a database using the [CREATE EXTENSION](../sql_commands/CREATE_EXTENSION.html) command. You remove a module from a database with [DROP EXTENSION](../sql_commands/DROP_EXTENSION.html).

The following Greenplum Database and PostgreSQL `contrib` modules are installed; refer to the linked module documentation for usage instructions.

-   [advanced\_password\_check](adv_passwd_check.html) Provides password quality checking and policy definition for Greenplum Database.
-   [auto\_explain](auto-explain.html) Provides a means for logging execution plans of slow statements automatically.
-   [btree\_gin](btree_gin.html) - Provides sample generalized inverted index \(GIN\) operator classes that implement B-tree equivalent behavior for certain data types.
-   [citext](citext.html) - Provides a case-insensitive, multibyte-aware text data type.
-   [dblink](dblink.html) - Provides connections to other Greenplum databases.
-   [diskquota](diskquota.html) - Allows administrators to set disk usage quotas for Greenplum Database roles and schemas.
-   [fuzzystrmatch](fuzzystrmatch.html) - Determines similarities and differences between strings.
-   [gp\_array\_agg](gp_array_agg.html) - Implements a parallel `array_agg()` aggregate function for Greenplum Database.
-   [gp\_check\_functions](gp_check_functions.html) - Provides views to check for orphaned and missing relation files and a user-defined function to move orphaned files.
-   [gp\_legacy\_string\_agg](gp_legacy_string_agg.html) - Implements a legacy, single-argument `string_agg()` aggregate function that was present in Greenplum Database 5.
-   [gp\_parallel\_retrieve\_cursor](gp_parallel_retrieve_cursor.html) - Provides extended cursor functionality to retrieve data, in parallel, directly from Greenplum Database segments.
-   [gp\_percentile\_agg](gp_percentile_agg.html) - Improves GPORCA performance for ordered-set aggregate functions.
-   [gp_pitr](gp_pitr.html.md) - Supports implementing Point-in-Time Recovery for Greenplum Database 6.
-   [gp\_sparse\_vector](gp_sparse_vector.html) - Implements a Greenplum Database data type that uses compressed storage of zeros to make vector computations on floating point numbers faster.
-   [greenplum\_fdw](greenplum_fdw.html) - Provides a foreign data wrapper \(FDW\) for accessing data stored in one or more external Greenplum Database clusters.
-   [gp_subtransaction_overflow](gp_subtransaction_overflow.html) - Provides a view and user-defined function for querying for suboverflowed backends.
-   [hstore](hstore.html) - Provides a data type for storing sets of key/value pairs within a single PostgreSQL value.
-   [ip4r](ip4r.html) - Provides data types for operations on IPv4 and IPv6 IP addresses.
-   [ltree](ltree.html) - Provides data types for representing labels of data stored in a hierarchical tree-like structure.
-   [orafce](orafce_ref.html) - Provides Greenplum Database-specific Oracle SQL compatibility functions.
-   [pageinspect](pageinspect.html) - Provides functions for low level inspection of the contents of database pages; available to superusers only.
-   [pg\_trgm](pg_trgm.html) - Provides functions and operators for determining the similarity of alphanumeric text based on trigram matching. The module also provides index operator classes that support fast searching for similar strings.
-   [pgcrypto](pgcrypto.html) - Provides cryptographic functions for Greenplum Database.
-   [postgres\_fdw](postgres_fdw.html) - Provides a foreign data wrapper \(FDW\) for accessing data stored in an external PostgreSQL or Greenplum database.
-   [postgresql-hll](postgresql-hll.html) - Provides HyperLogLog data types for PostgreSQL and Greenplum Database.
-   [sslinfo](sslinfo.html) - Provides information about the SSL certificate that the current client provided when connecting to Greenplum.
-   [tablefunc](tablefunc.html) - Provides various functions that return tables (multiple rows).
-   [timestamp9](timestamp9.html) - Provides an efficient nanosecond-precision timestamp data type for Greenplum Database.
-   [uuid-ossp](uuid-ossp.html) - Provides functions to generate universally unique identifiers (UUIDs).
