---
title: Migrating Data from Greenplum 4.3 or 5 to Greenplum 6 
---

You can migrate data from Greenplum Database 4.3 or 5 to Greenplum 6 using the standard backup and restore procedures, `gpbackup` and `gprestore`, or by using `gpcopy` for VMware Greenplum.

> **Note** Open source Greenplum Database is available only for Greenplum Database 5 and later.

> **Note** You can upgrade a Greenplum Database 5.28 system directly to Greenplum 6.9 or later using [gpupgrade](https://docs.vmware.com/en/VMware-Greenplum-Upgrade/index.html). You cannot upgrade a Greenplum Database 4.3 system directly to Greenplum 6.

This topic identifies known issues you may encounter when moving data from Greenplum 4.3 to Greenplum 6. You can work around these problems by making needed changes to your Greenplum 4.3 databases so that you can create backups that can be restored successfully to Greenplum 6.

-   [Preparing the Greenplum 6 Cluster](#prep-gpdb6)
-   [Preparing Greenplum 4.3 and 5 Databases for Backup](#prep-gpdb4)
-   [Backing Up and Restoring a Database](#backup-and-restore)
-   [Completing the Migration](#after-migration)

**Parent topic:** [Upgrading to Greenplum 6](upgrade_intro.html)

## <a id="prep-gpdb6"></a>Preparing the Greenplum 6 Cluster 

-   Install and initialize a new Greenplum Database 6 cluster using the version 6 `gpinitsystem` utility.

    > **Note** `gprestore` only supports restoring data to a cluster that has an identical number of hosts and an identical number of segments per host, with each segment having the same `content_id` as the segment in the original cluster. Use `gpcopy` \(VMware Greenplum\) if you need to migrate data to a different-sized Greenplum 6 cluster.

    > **Note** Set the Greenplum Database 6 timezone to a value that is compatible with your host systems. Setting the Greenplum Database timezone prevents Greenplum Database from selecting a timezone each time the cluster is restarted. See [Configuring Timezone and Localization Settings](localization.html) for more information.

-   Install the latest release of the Greenplum Backup and Restore utilities, available to download from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb-backup-restore) or [github](https://github.com/greenplum-db/gpbackup/releases).
-   If you intend to install Greenplum Database 6 on the same hardware as your 4.3 system, you will need enough disk space to accommodate over five times the original data set \(two full copies of the primary and mirror data sets, plus the original backup data in ASCII format\) in order to migrate data with `gpbackup` and `gprestore`. Keep in mind that the ASCII backup data will require more disk space than the original data, which may be stored in compressed binary format. Offline backup solutions such as Dell EMC Data Domain can reduce the required disk space on each host.

    If you want to migrate your data on the same hardware but do not have enough free disk space on your host systems, `gpcopy` for VMware Greenplum provides the `--truncate-source-after` option to truncate each source table after copying the table to the destination cluster and validating that the copy succeeded. This reduces the amount of free space needed to migrate clusters that reside on the same hardware. See [Migrating Data with gpcopy](../admin_guide/managing/gpcopy-migrate.html) for more information.

-   Install any external modules used in your Greenplum 4.3 system in the Greenplum 6 system before you restore the backup, for example MADlib or PostGIS. If versions of the external modules are not compatible, you may need to exclude tables that reference them when restoring the Greenplum 4.3 backup to Greenplum 6.
-   The Greenplum 4.3 Oracle Compatibility Functions module is not compatible with Greenplum 6. Uninstall the module from Greenplum 4.3 by running the `uninstall_orafunc.sql` script before you back up your databases:

    ```
    $ $GPHOME/share/postgresql/contrib/uninstall_orafunc.sql
    ```

    You will also need to drop any dependent database objects that reference compatibility functions.

    Install the Oracle Compatibility Functions in Greenplum 6 by creating the `orafce` module in each database where you require the functions:

    ```
    $ psql -d <dbname> -c 'CREATE EXTENSION orafce'
    ```

    See [Installing Additional Supplied Modules](install_modules.html) for information about installing `orafce` and other modules.

-   When restoring language-based user-defined functions, the shared object file must be in the location specified in the `CREATE FUNCTION` SQL command and must have been recompiled on the Greenplum 6 system. This applies to user-defined functions, user-defined types, and any other objects that use custom functions, such as aggregates created with the `CREATE AGGREGATE` command.
-   Greenplum 6 provides *resource groups*, an alternative to managing resources using resource queues. Setting the `gp_resource_manager` server configuration parameter to `queue` or `group` selects the resource management scheme the Greenplum Database system will use. The default is `queue`, so no action is required when you move from Greenplum version 4.3 to version 6. To more easily transition from resource queues to resource groups, you can set resource groups to allocate and manage memory in a way that is similar to resource queue memory management. To select this feature, set the `MEMORY_LIMIT` and `MEMORY_SPILL_RATIO` attributes of your resource groups to 0. See [Using Resource Groups](../admin_guide/workload_mgmt_resgroups.html) for information about enabling and configuring resource groups.
-   Filespaces are removed in Greenplum 6. When creating a tablespace, note that different tablespace locations for a primary-mirror pair is no longer supported in Greenplum 6. See [Creating and Managing Tablespaces](../admin_guide/ddl/ddl-tablespace.html) for information about creating and configuring tablespaces.

## <a id="prep-gpdb4"></a>Preparing Greenplum 4.3 and 5 Databases for Backup 

> **Note** A Greenplum 4 system must be at least version 4.3.22 to use the `gpbackup` and `gprestore` utilities. A Greenplum 5 system must be at least version 5.5. Be sure to use the latest release of the backup and restore utilities, available for download from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb-backup-restore) or [github](https://github.com/greenplum-db/gpbackup/releases).

> **Important** Make sure that you have a complete backup of all data in the Greenplum Database 4.3 or 5 cluster, and that you can successfully restore the Greenplum Database cluster if necessary.

Following are some issues that are known to cause errors when restoring a Greenplum 4.3 or 5 backup to Greenplum 6. Keep a list of any changes you make to the Greenplum 4.3 or 5 database to enable migration so that you can fix them in Greenplum 6 after restoring the backup.

-   If you have configured PXF in your Greenplum Database 5 installation, review [Migrating PXF from Greenplum 5](../pxf/migrate_5to6.html) to plan for the PXF migration.
-   Greenplum Database version 6 removes support for the `gphdfs` protocol. If you have created external tables that use gphdfs, remove the external table definitions and \(optionally\) recreate them to use Greenplum Platform Extension Framework \(PXF\) before you migrate the data to Greenplum 6. Refer to [Migrating gphdfs External Tables to PXF](../pxf/gphdfs-pxf-migrate.html) in the PXF documentation for the migration procedure.
-   References to catalog tables or their attributes can cause a restore to fail due to catalog changes from Greenplum 4.3 or 5 to Greenplum 6. Here are some catalog issues to be aware of when migrating to Greenplum 6:
    -   In the `pg_class` system table, the `reltoastidxid` column has been removed.
    -   In the `pg_stat_replication` system table, the `procpid` column is renamed to `pid`.
    -   In the `pg_stat_activity` system table, the `procpid` column is renamed to `pid`. The `current_query` column is replaced by two columns: `state` \(the state of the backend\), and `query` \(the last run query, or currently running query if `state` is `active`\).
    -   In the `gp_distribution_policy` system table, the `attrnums` column is renamed to `distkey` and its data type is changed to `int2vector`. A backend function `pg_get_table_distributedby()` was added to get the distribution policy for a table as a string.
    -   The `__gp_localid` and `__gp_masterid` columns are removed from the `session_level_memory_consumption` system view in Greenplum 6. The underlying external tables and functions are removed from the `gp_toolkit` schema.
    -   Filespaces are removed in Greenplum 6. The `pg_filespace` and `pg_filespace_entry` system tables are removed. Any reference to `pg_filespace` or `pg_filespace_entry` will fail in Greenplum 6.
    -   Restoring a Greenplum 4 backup can fail due to lack of dependency checking in Greenplum 4 catalog tables. For example, restoring a UDF can fail if it references a custom data type that is created later in the backup file.
-   The `INTO error_table` clause of the `CREATE EXTERNAL TABLE` and `COPY` commands was deprecated in Greenplum 4.3 and is unsupported in Greenplum 5 and 6. Remove this clause from any external table definitions before you create a backup of your Greenplum 4.3 system. The `ERROR_TABLE` parameter of the `gpload` utility load control YAML file must also be removed from any `gpload` YAML files before you run `gpload`.
-   The `int4_avg_accum()` function signature changed in Greenplum 6 from `int4_avg_accum(bytea, integer)` to `int4_avg_accum(bigint[], integer)`. This function is the state transition function \(*sfunc*\) called when calculating the average of a series of 4-byte integers. If you have created a custom aggregate in a previous Greenplum release that called the built-in `int4_avg_accum()` function, you will need to revise your aggregate for the new signature.
-   The `string_agg(expression)` function has been removed from Greenplum 6. The function concatenates text values into a string. You can replace the single argument function with the function `string_agg(expression, delimiter)` and specify an empty string as the `delimiter`, for example `string_agg(txt_col1, '').`
-   The `offset` argument of the `lag(value, offset[, default])` and `lead(value, offset[, default])` window functions has changed from data type `int8` (`bigint`) in Greenplum 4.3 and 5 to `int4` (`integer`) in Greenplum 6. If you used these functions to create views in Greenplum 5, you must remove the views before you migrate to Greenplum 6. Refer to [About Migrating Views Created with lag()/lead() Functions](#laglead) for additional information and migration actions.
-   `gpbackup` saves the distribution policy and distribution key for each table in the backup so that data can be restored to the same segment. If a table's distribution key in the Greenplum 4.3 or 5 database is incompatible with Greenplum 6, `gprestore` cannot restore the table to the correct segment in the Greenplum 6 database. This can happen if the distribution key in the older Greenplum release has columns with data types not allowed in Greenplum 6 distribution keys, or if the data representation for data types has changed or is insufficient for Greenplum 6 to generate the same hash value for a distribution key. You should correct these kinds of problems by altering distribution keys in the tables before you back up the Greenplum database.
-   Greenplum 6 requires primary keys and unique index keys to match a table's distribution key. The leaf partitions of partitioned tables must have the same distribution policy as the root partition. These known issues should be corrected in the source Greenplum database before you back up the database:
    -   If the primary key is different than the distribution key for a table, alter the table to either remove the primary key or change the primary key to match the distribution key.
    -   If the key columns for a unique index are not a subset of the distribution key columns, before you back up the source database, drop the index and, optionally, recreate it with a compatible key.
    -   If a partitioned table in the source database has a `DISTRIBUTED BY` distribution policy, but has leaf partitions that are `DISTRIBUTED RANDOMLY`, alter the leaf tables to match the root table distribution policy before you back up the source database.
-   In Greenplum 4.3, the name provided for a constraint in a `CREATE TABLE` command was the name of the index created to enforce the constraint, which could lead to indexes having the same name. In Greenplum 6, duplicate index names are not allowed; restoring from a Greenplum 4.3 backup that has duplicate index names will generate errors.
-   Columns of type `abstime`, `reltime`, `tinterval`, `money`, or `anyarray` are not supported as distribution keys in Greenplum 6.

    If you have tables distributed on columns of type `abstime`, `reltime`, `tinterval`, `money`, or `anyarray`, use the `ALTER TABLE` command to set the distribution to `RANDOM` before you back up the database. After the data is restored, you can set a new distribution policy.

-   In Greenplum 4.3 and 5, it was possible to `ALTER` a table that has a primary key or unique index to be `DISTRIBUTED RANDOMLY`. Greenplum 6 does not permit tables `DISTRIBUTED RANDOMLY` to have primary keys or unique indexes. Restoring such a table from a Greenplum 4.3 or 5 backup will cause an error.
-   Greenplum 6 no longer automatically converts from the deprecated timestamp format `YYYYMMDDHH24MISS`. The format could not be parsed unambiguously in previous Greenplum Database releases. You can still specify the `YYYYMMDDHH24MISS` format in conversion functions such as `to_timestamp` and `to_char` for compatibility with other database systems. You can use input formats for converting text to date or timestamp values to avoid unexpected results or query execution failures. For example, this `SELECT` command returns a timestamp in Greenplum Database 5 and fails in 6.

    ```
    SELECT to_timestamp('20190905140000');
    ```

    To convert the string to a timestamp in Greenplum Database 6, you must use a valid format. Both of these commands return a timestamp in Greenplum Database 6. The first example explicitly specifies a timestamp format. The second example uses the string in a format that Greenplum Database recognizes.

    ```
    SELECT to_timestamp('20190905140000','YYYYMMDDHH24MISS');
    SELECT to_timestamp('201909051 40000');
    ```

    The timestamp issue also applies when you use the `::` syntax. In Greenplum Database 6, the first command returns an error. The second command returns a timestamp.

    ```
    SELECT '20190905140000'::timestamp ;
    SELECT '20190905 140000'::timestamp ;
    ```

-   Creating a table using the `CREATE TABLE AS` command in Greenplum 4.3 or 5 could create a table with a duplicate distribution key. The `gpbackup` utility saves the table to the backup using a `CREATE TABLE` command that lists the duplicate keys in the `DISTRIBUTED BY` clause. Restoring this backup will cause a duplicate distribution key error. The `CREATE TABLE AS` command was fixed in Greenplum 5.10 to disallow duplicate distribution keys.
-   Greenplum 4.3 supports foreign key constraints on columns of different types, for example, `numeric` and `bigint`, with implicit type conversion. Greenplum 5 and 6 do not support implicit type conversion. Restoring a table with a foreign key on columns with different data types causes an error.
-   Only Boolean operators can use Boolean negators. In Greenplum Database 4.3 and 5 it was possible to create a non-Boolean operator that specifies a Boolean negator function. For example, this `CREATE OPERATOR` command creates an integer `@@` operator with a Boolean negator:

    ```
    CREATE OPERATOR public.@@ (
            PROCEDURE = int4pl,
            LEFTARG = integer,
            RIGHTARG = integer,
            NEGATOR = OPERATOR(public.!!)
    );
    ```

    If you restore a backup containing an operator like this to a Greenplum 6 system, `gprestore` produces an error: `ERROR: only boolean operators can have negators (SQLSTATE 42P13)`.

-   In Greenplum Database 4.3 and 5, the undocumented server configuration parameter `allow_system_table_mods` could have a value of `none`, `ddl`, `dml`, or `all`. In Greenplum 6, this parameter has changed to a Boolean value, with a default value of `false`. If there are any references to this parameter in the source database, remove them to prevent errors during the restore.

### <a id="laglead"></a>About Migrating Views Created with lag()/lead() Functions

The `offset` argument of the `lag(value, offset[, default])` and `lead(value, offset[, default])` window functions has changed from data type `int8` (`bigint`) in Greenplum 4.3 and 5 to `int4` (`integer`) in Greenplum 6. If you used these functions to create views in Greenplum 5, you must remove the views before you migrate to Greenplum 6.

In addition to the `offset` argument data type change, Greenplum Database 6 removes all data-type-specific `lag()` and `lead()` functions:

```
<data-type> lag( value <data-type>[, offset bigint[, default <data-type>]] )
```
```
<data-type> lead( value <data-type>[, offset bigint[, default <data-type>]] )
```

And replaces them with these more generic function signatures:

```
anyelement lag( value anyelement[, offset integer[, default anyelement]] )
```
```
anyelement lead( value anyelement[, offset integer[, default anyelement]] )
```

To migrate to the `lag()` and `lead()` functions in Greenplum Database 6, you must:

1. Identify all views in your Greenplum Database 5 installation that were created using the two- or three-argument `lag()` or `lead()` functions. Run the following SQL command to identify the views:

    ``` sql
    SELECT ev_class::regclass::text viewname
      FROM pg_rewrite pgr
      WHERE ev_action ~
        (SELECT $$:winfnoid ($$||string_agg(oid::text,'|')||$$) :$$
          FROM (SELECT DISTINCT oid FROM pg_catalog.pg_proc WHERE (proname, pronamespace) IN
          (('lag', 11), ('lead', 11)) AND proargtypes[1]=20)s1);
    ```

1. For each view that you identify:

    1. Note the command that you used to create the view.
    1. Drop the view in your Greenplum 5 installation.
    1. After you migrate to Greenplum 6:

        1. Recreate the view, if desired, in your Greenplum 6 installation; be sure to specify an `integer`-type `offset` argument to the function. (You are not required to modify the `value` or `default` argument types in the function because an `anyelement` type accepts any data type.)
        1. If you have any scripts that invoke `lag()` or `lead()` functions, ensure that that they reference an `integer`-type `offset`.

## <a id="backup-and-restore"></a>Backing Up and Restoring a Database 

First use `gpbackup` to create a `--metadata-only` backup from the source Greenplum database and restore it to the Greenplum 6 system. This helps find any additional problems that are not identified in [Preparing Greenplum 4.3 and 5 Databases for Backup](#prep-gpdb4). Refer to the [Greenplum Backup and Restore documentation](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html) for syntax and examples for the `gpbackup` and `gprestore` utilities.

Review the `gprestore` log file for error messages and correct any remaining problems in the source Greenplum database.

When you are able to restore a metadata backup successfully, create the full backup and then restore it to the Greenplum 6 system, or use `gpcopy` \(VMware Greenplum\) to transfer the data. If needed, use the `gpbackup` or `gprestore` filter options to omit schemas or tables that cannot be restored without error.

If you use `gpcopy` to migrate VMware Greenplum data, initiate the `gpcopy` operation from the Greenplum 4.3.26 \(or later\) or the 5.9 \(or later\) cluster. See [Migrating Data with gpcopy](../admin_guide/managing/gpcopy-migrate.html) for more information.

> **Important** When you restore a backup taken from a Greenplum Database 4.3 or 5 system, `gprestore` warns that the restore will use legacy hash operators when loading the data. This is because Greenplum 6 has new hash algorithms that map distribution keys to segments, but the data in the backup set must be restored to the same segments as the cluster from which the backup was taken. The `gprestore` utility sets the `gp_use_legacy_hashops` server configuration parameter to `on` when restoring to Greenplum 6 from an earlier version so that the restored tables are created using the legacy operator classes instead of the new default operator classes.

After restoring, you can redistribute these tables with the `gp_use_legacy_hashops` parameter set to `off` so that the tables use the new Greenplum 6 hash operators. See [Working With Hash Operator Classes in Greenplum 6](#redistribute) for more information and examples.

## <a id="after-migration"></a>Completing the Migration 

Migrate any tables you skipped during the restore using other methods, for example using the `COPY TO` command to create an external file and then loading the data from the external file into Greenplum 6 with the `COPY FROM` command.

Recreate any objects you dropped in the Greenplum 4.3 or 5 database to enable migration, such as external tables, indexes, user-defined functions, or user-defined aggregates.

Here are some additional items to consider to complete your migration to Greenplum 6.

-   Greenplum Database 5 and 6 remove automatic casts between the text type and other data types. After you migrate from Greenplum Database version 4.3 to version 6, this changed behavior may impact existing applications and queries. Refer to [About Implicit Text Casting in Greenplum Database](43x_to_5x.html) for information, including a discussion about VMware supported and unsupported workarounds.
-   After migrating data you may need to modify SQL scripts, administration scripts, and user-defined functions as necessary to account for changes in Greenplum Database version 6. Review the [VMware Greenplum 6.0.0 Release Notes](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/GUID-relnotes-release-notes.html#release-6.0.0) for features and changes that may necessitate post-migration tasks.
-   To use the new Greenplum 6 default hash operator classes, use the command `ALTER TABLE <table> SET DISTRIBUTED BY (<key>)` to redistribute tables restored from Greenplum 4.3 or 5 backups. The `gp_use_legacy_hashops` parameter must be set to `off` when you run the command. See [Working With Hash Operator Classes in Greenplum 6](#redistribute) for more information about hash operator classes.

### <a id="redistribute"></a>Working With Hash Operator Classes in Greenplum 6 

Greenplum 6 has new *jump consistent* hash operators that map distribution keys for distributed tables to the segments. The new hash operators enable faster database expansion because they don't require redistributing rows unless they map to a different segment. The hash operators used in Greenplum 4.3 and 5 are present in Greenplum 6 as non-default legacy hash operator classes. For example, for integer columns, the new hash operator class is named `int_ops` and the legacy operator class is named `cdbhash_int_ops`.

This example creates a table using the legacy hash operator class `cdbhash_int_ops`.

```
test=# SET gp_use_legacy_hashops=on;
SET            
test=# CREATE TABLE t1 (
    c1 integer,
    c2 integer,
    p integer
) DISTRIBUTED BY (c1);
CREATE TABLE
test=# \d+ t1
                          Table "public.t1"
 Column |  Type   | Modifiers | Storage | Stats target | Description
--------+---------+-----------+---------+--------------+-------------
 c1     | integer |           | plain   |              |
 c2     | integer |           | plain   |              |
 p      | integer |           | plain   |              |
Distributed by: (c1)
```

Notice that the distribution key is `c1`. If the `gp_use_legacy_hashops` parameter is `on` and the operator class is a legacy operator class, the operator class name is not shown. However, if `gp_use_legacy_hashops` is `off`, the legacy operator class name is reported with the distribution key.

```
test=# SET gp_use_legacy_hashops=off;
SET
test=# \d+ t1
                          Table "public.t1"
 Column |  Type   | Modifiers | Storage | Stats target | Description
--------+---------+-----------+---------+--------------+-------------
 c1     | integer |           | plain   |              |
 c2     | integer |           | plain   |              |
 p      | integer |           | plain   |              |
Distributed by: (c1 cdbhash_int4_ops)
```

The operator class name is reported only when it does not match the setting of the `gp_use_legacy_hashops` parameter.

To change the table to use the new jump consistent operator class, use the `ALTER TABLE` command to redistribute the table with the `gp_use_legacy_hashops` parameter set to `off`.

> **Note** Redistributing tables with a large amount of data can take a long time.

```
test=# SHOW gp_use_legacy_hashops;
 gp_use_legacy_hashops
-----------------------
 off
(1 row)

test=# ALTER TABLE t1 SET DISTRIBUTED BY (c1);
ALTER TABLE
test=# \d+ t1
                          Table "public.t1"
 Column |  Type   | Modifiers | Storage | Stats target | Description
--------+---------+-----------+---------+--------------+-------------
 c1     | integer |           | plain   |              |
 c2     | integer |           | plain   |              |
 p      | integer |           | plain   |              |
Distributed by: (c1)
```

To verify the default jump consistent operator class has been used, set `gp_use_legacy_hashops` to on before you show the table definition.

```
test=# SET gp_use_legacy_hashops=on;
SET
test=# \d+ t1
                          Table "public.t1"
 Column |  Type   | Modifiers | Storage | Stats target | Description
--------+---------+-----------+---------+--------------+-------------
 c1     | integer |           | plain   |              |
 c2     | integer |           | plain   |              |
 p      | integer |           | plain   |              |
Distributed by: (c1 int4_ops)
```

