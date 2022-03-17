---
title: Parallel Backup with gpbackup and gprestore 
---

`gpbackup` and `gprestore` are Greenplum Database utilities that create and restore backup sets for Greenplum Database. By default, `gpbackup` stores only the object metadata files and DDL files for a backup in the Greenplum Database master data directory. Greenplum Database segments use the `COPY ... ON SEGMENT` command to store their data for backed-up tables in compressed CSV data files, located in each segment's backups directory.

The backup metadata files contain all of the information that `gprestore` needs to restore a full backup set in parallel. Backup metadata also provides the framework for restoring only individual objects in the data set, along with any dependent objects, in future versions of `gprestore`. \(See [Understanding Backup Files](#topic_xnj_b4c_tbb) for more information.\) Storing the table data in CSV files also provides opportunities for using other restore utilities, such as `gpload`, to load the data either in the same cluster or another cluster. By default, one file is created for each table on the segment. You can specify the `--leaf-partition-data` option with `gpbackup` to create one data file per leaf partition of a partitioned table, instead of a single file. This option also enables you to filter backup sets by leaf partitions.

Each `gpbackup` task uses a single transaction in Greenplum Database. During this transaction, metadata is backed up on the master host, and data for each table on each segment host is written to CSV backup files using `COPY ... ON SEGMENT` commands in parallel. The backup process acquires an `ACCESS SHARE` lock on each table that is backed up.

For information about the `gpbackup` and `gprestore` utility options, see [gpbackup](../../utility_guide/ref/gpbackup.html) and [gprestore](../../utility_guide/ref/gprestore.html).

-   **[Requirements and Limitations](../managing/backup-gpbackup.html)**  

-   **[Objects Included in a Backup or Restore](../managing/backup-gpbackup.html)**  

-   **[Performing Basic Backup and Restore Operations](../managing/backup-gpbackup.html)**  

-   **[Filtering the Contents of a Backup or Restore](../managing/backup-gpbackup.html)**  

-   **[Configuring Email Notifications](../managing/backup-gpbackup.html)**  

-   **[Understanding Backup Files](../managing/backup-gpbackup.html)**  

-   **[Creating and Using Incremental Backups with gpbackup and gprestore](../managing/backup-gpbackup-incremental.html)**  

-   **[Using gpbackup and gprestore with BoostFS](../managing/backup-boostfs.html)**  

-   **[Using gpbackup Storage Plugins](../managing/backup-plugins.html)**  

-   **[Backup/Restore Storage Plugin API](../managing/backup-plugin-api.html)**  


**Parent topic:**[Backing Up and Restoring Databases](../managing/backup-main.html)

## <a id="topic_vh5_1hd_tbb"></a>Requirements and Limitations 

The `gpbackup` and `gprestore` utilities are compatible with these Greenplum Database versions:

-   Pivotal Greenplum Database 4.3.22 and later
-   Pivotal Greenplum Database 5.5.0 and later
-   Tanzu Greenplum 6.0.0 and later

`gpbackup` and `gprestore` have the following limitations:

-   If you create an index on a parent partitioned table, `gpbackup` does not back up that same index on child partitioned tables of the parent, as creating the same index on a child would cause an error. However, if you exchange a partition, `gpbackup` does not detect that the index on the exchanged partition is inherited from the new parent table. In this case, `gpbackup` backs up conflicting `CREATE INDEX` statements, which causes an error when you restore the backup set.
-   You can execute multiple instances of `gpbackup`, but each execution requires a distinct timestamp.
-   Database object filtering is currently limited to schemas and tables.
-   When backing up a partitioned table where some or all leaf partitions are in different schemas from the root partition, the leaf partition table definitions, including the schemas, are backed up as metadata. This occurs even if the backup operation specifies that schemas that contain the leaf partitions should be excluded. To control data being backed up for this type of partitioned table in this situation, use the `--leaf-partition-data` option.
    -   If the `--leaf-partition-data` option is not specified, the leaf partition data is also backed up even if the backup operation specifies that the leaf partition schemas should excluded.
    -   If the `--leaf-partition-data` option is specified, the leaf partition data is not be backed up if the backup operation specifies that the leaf partition schemas should excluded. Only the metadata for leaf partition tables are backed up.
-   If you use the `gpbackup --single-data-file` option to combine table backups into a single file per segment, you cannot perform a parallel restore operation with `gprestore` \(cannot set `--jobs` to a value higher than 1\).
-   You cannot use the `--exclude-table-file` with `--leaf-partition-data`. Although you can specify leaf partition names in a file specified with `--exclude-table-file`, `gpbackup` ignores the partition names.
-   Backing up a database with `gpbackup` while simultaneously running DDL commands might cause `gpbackup` to fail, in order to ensure consistency within the backup set. For example, if a table is dropped after the start of the backup operation, `gpbackup` exits and displays the error message `ERROR: relation <schema.table> does not exist`.

    `gpbackup` might fail when a table is dropped during a backup operation due to table locking issues. `gpbackup` generates a list of tables to back up and acquires an `ACCESS SHARED` lock on the tables. If an `EXCLUSIVE LOCK` is held on a table, `gpbackup` acquires the `ACCESS SHARED` lock after the existing lock is released. If the table no longer exists when `gpbackup` attempts to acquire a lock on the table, `gpbackup` exits with the error message.

    For tables that might be dropped during a backup, you can exclude the tables from a backup with a `gpbackup` table filtering option such as `--exclude-table` or `--exclude-schema`.

-   A backup created with `gpbackup` can only be restored to a Greenplum Database cluster with the same number of segment instances as the source cluster. If you run `gpexpand` to add segments to the cluster, backups you made before starting the expand cannot be restored after the expansion has completed.

**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

## <a id="topic_x3s_lqj_tbb"></a>Objects Included in a Backup or Restore 

The following table lists the objects that are backed up and restored with `gpbackup` and `gprestore`. Database objects are backed up for the database you specify with the `--dbname` option. Global objects \(Greenplum Database system objects\) are also backed up by default, but they are restored only if you include the `--with-globals` option to `gprestore`.

<table class="table frame-all" id="topic_x3s_lqj_tbb__table_vqq_3rj_tbb"><caption><span class="table--title-label">Table 1. </span><span class="title">Objects that are backed up and restored</span></caption><colgroup><col style="width:50%"><col style="width:50%"></colgroup><thead class="thead">
              <tr class="row">
                <th class="entry colsep-1 rowsep-1" id="topic_x3s_lqj_tbb__table_vqq_3rj_tbb__entry__1">Database (for database specified with <code class="ph codeph">--dbname</code>)</th>
                <th class="entry colsep-1 rowsep-1" id="topic_x3s_lqj_tbb__table_vqq_3rj_tbb__entry__2">Global (requires the <code class="ph codeph">--with-globals</code> option to
                  restore)</th>
              </tr>
            </thead><tbody class="tbody">
              <tr class="row">
                <td class="entry colsep-1 rowsep-1" headers="topic_x3s_lqj_tbb__table_vqq_3rj_tbb__entry__1">
                  <ul class="ul" id="topic_x3s_lqj_tbb__ul_kpk_yrj_tbb">
                    <li class="li">Session-level configuration parameter settings (GUCs)</li>
                    <li class="li">Schemas, see <a class="xref" href="#topic_x3s_lqj_tbb__schema_note">Note</a></li>
                    <li class="li">Procedural language extensions</li>
                    <li class="li">Sequences</li>
                    <li dir="ltr" class="li">Comments</li>
                    <li dir="ltr" class="li">Tables</li>
                    <li class="li">Indexes</li>
                    <li dir="ltr" class="li">Owners</li>
                    <li dir="ltr" class="li">Writable External Tables (DDL only) </li>
                    <li dir="ltr" class="li">Readable External Tables (DDL only) </li>
                    <li dir="ltr" class="li">Functions</li>
                    <li dir="ltr" class="li">Aggregates</li>
                    <li dir="ltr" class="li">Casts</li>
                    <li dir="ltr" class="li">Types</li>
                    <li dir="ltr" class="li">Views</li>
                    <li class="li">Materialized Views (DDL only)</li>
                    <li dir="ltr" class="li">Protocols</li>
                    <li dir="ltr" class="li">Triggers. (While Greenplum Database does not support triggers, any
                      trigger definitions that are present are backed up and restored.)</li>
                    <li dir="ltr" class="li">Rules</li>
                    <li dir="ltr" class="li">Domains</li>
                    <li dir="ltr" class="li">Operators, operator families, and operator classes</li>
                    <li dir="ltr" class="li">Conversions</li>
                    <li class="li">Extensions</li>
                    <li dir="ltr" class="li">Text search parsers, dictionaries, templates, and
                      configurations</li>
                  </ul>
                </td>
                <td class="entry colsep-1 rowsep-1" headers="topic_x3s_lqj_tbb__table_vqq_3rj_tbb__entry__2">
                  <ul class="ul" id="topic_x3s_lqj_tbb__ul_d2t_wrj_tbb">
                    <li class="li">Tablespaces</li>
                    <li class="li">Databases</li>
                    <li class="li">Database-wide configuration parameter settings (GUCs)</li>
                    <li class="li">Resource group definitions</li>
                    <li class="li">Resource queue definitions</li>
                    <li class="li">Roles</li>
                    <li class="li"><code class="ph codeph">GRANT</code> assignments of roles to databases</li>
                  </ul>
                </td>
              </tr>
            </tbody></table>

**Note:** These schemas are not included in a backup.

-   `gp_toolkit`
-   `information_schema`
-   `pg_aoseg`
-   `pg_bitmapindex`
-   `pg_catalog`
-   `pg_toast*`
-   `pg_temp*`

When restoring to an existing database, `gprestore` assumes the `public` schema exists when restoring objects to the `public` schema. When restoring to a new database \(with the `--create-db` option\), `gprestore` creates the `public` schema automatically when creating a database with the `CREATE DATABASE` command. The command uses the `template0` database that contains the `public` schema.

See also [Understanding Backup Files](#topic_xnj_b4c_tbb).

**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

## <a id="topic_qgj_b3d_tbb"></a>Performing Basic Backup and Restore Operations 

To perform a complete backup of a database, as well as Greenplum Database system metadata, use the command:

```
$ gpbackup --dbname <database_name>
```

For example:

```
$ gpbackup --dbname demo
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Starting backup of database demo
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Backup Timestamp = 20180105112754
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Backup Database = demo
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Backup Type = Unfiltered Compressed Full Backup
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Gathering list of tables for backup
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Acquiring ACCESS SHARE locks on tables
Locks acquired:  6 / 6 [================================================================] 100.00% 0s
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Gathering additional table metadata
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Writing global database metadata
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Global database metadata backup complete
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Writing pre-data metadata
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Pre-data metadata backup complete
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Writing post-data metadata
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Post-data metadata backup complete
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Writing data to file
Tables backed up:  3 / 3 [==============================================================] 100.00% 0s
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Data backup complete
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Found neither /usr/local/greenplum-db/./bin/gp_email_contacts.yaml nor /home/gpadmin/gp_email_contacts.yaml
20180105:11:27:54 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Email containing gpbackup report /gpmaster/seg-1/backups/20180105/20180105112754/gpbackup_20180105112754_report will not be sent
20180105:11:27:55 gpbackup:gpadmin:centos6.localdomain:002182-[INFO]:-Backup completed successfully
```

The above command creates a file that contains global and database-specific metadata on the Greenplum Database master host in the default directory, `$COORDINATOR_DATA_DIRECTORY/backups/<YYYYMMDD>/<YYYYMMDDHHMMSS>/`. For example:

```
$ ls /gpmaster/gpsne-1/backups/20180105/20180105112754
gpbackup_20180105112754_config.yaml   gpbackup_20180105112754_report
gpbackup_20180105112754_metadata.sql  gpbackup_20180105112754_toc.yaml
```

By default, each segment stores each table's data for the backup in a separate compressed CSV file in `<seg_dir>/backups/<YYYYMMDD>/<YYYYMMDDHHMMSS>/`:

```
$ ls /gpdata1/gpsne0/backups/20180105/20180105112754/
gpbackup_0_20180105112754_17166.gz  gpbackup_0_20180105112754_26303.gz
gpbackup_0_20180105112754_21816.gz
```

To consolidate all backup files into a single directory, include the `--backup-dir` option. Note that you must specify an absolute path with this option:

```
$ gpbackup --dbname demo --backup-dir /home/gpadmin/backups
20171103:15:31:56 gpbackup:gpadmin:0ee2f5fb02c9:017586-[INFO]:-Starting backup of database demo
...
20171103:15:31:58 gpbackup:gpadmin:0ee2f5fb02c9:017586-[INFO]:-Backup completed successfully
$ find /home/gpadmin/backups/ -type f
/home/gpadmin/backups/gpseg0/backups/20171103/20171103153156/gpbackup_0_20171103153156_16543.gz
/home/gpadmin/backups/gpseg0/backups/20171103/20171103153156/gpbackup_0_20171103153156_16524.gz
/home/gpadmin/backups/gpseg1/backups/20171103/20171103153156/gpbackup_1_20171103153156_16543.gz
/home/gpadmin/backups/gpseg1/backups/20171103/20171103153156/gpbackup_1_20171103153156_16524.gz
/home/gpadmin/backups/gpseg-1/backups/20171103/20171103153156/gpbackup_20171103153156_config.yaml
/home/gpadmin/backups/gpseg-1/backups/20171103/20171103153156/gpbackup_20171103153156_predata.sql
/home/gpadmin/backups/gpseg-1/backups/20171103/20171103153156/gpbackup_20171103153156_global.sql
/home/gpadmin/backups/gpseg-1/backups/20171103/20171103153156/gpbackup_20171103153156_postdata.sql
/home/gpadmin/backups/gpseg-1/backups/20171103/20171103153156/gpbackup_20171103153156_report
/home/gpadmin/backups/gpseg-1/backups/20171103/20171103153156/gpbackup_20171103153156_toc.yaml
```

When performing a backup operation, you can use the `--single-data-file` in situations where the additional overhead of multiple files might be prohibitive. For example, if you use a third party storage solution such as Data Domain with back ups.

**Note:** Backing up a materialized view does not back up the materialized view data. Only the materialized view definition is backed up.

### <a id="rest"></a>Restoring from Backup 

To use `gprestore` to restore from a backup set, you must use the `--timestamp` option to specify the exact timestamp value \(`YYYYMMDDHHMMSS`\) to restore. Include the `--create-db` option if the database does not exist in the cluster. For example:

```
$ dropdb demo
$ gprestore --timestamp 20171103152558 --create-db
20171103:15:45:30 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Restore Key = 20171103152558
20171103:15:45:31 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Creating database
20171103:15:45:44 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Database creation complete
20171103:15:45:44 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Restoring pre-data metadata from /gpmaster/gpsne-1/backups/20171103/20171103152558/gpbackup_20171103152558_predata.sql
20171103:15:45:45 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Pre-data metadata restore complete
20171103:15:45:45 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Restoring data
20171103:15:45:45 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Data restore complete
20171103:15:45:45 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Restoring post-data metadata from /gpmaster/gpsne-1/backups/20171103/20171103152558/gpbackup_20171103152558_postdata.sql
20171103:15:45:45 gprestore:gpadmin:0ee2f5fb02c9:017714-[INFO]:-Post-data metadata restore complete
```

If you specified a custom `--backup-dir` to consolidate the backup files, include the same `--backup-dir` option when using `gprestore` to locate the backup files:

```
$ dropdb demo
$ gprestore --backup-dir /home/gpadmin/backups/ --timestamp 20171103153156 --create-db
20171103:15:51:02 gprestore:gpadmin:0ee2f5fb02c9:017819-[INFO]:-Restore Key = 20171103153156
...
20171103:15:51:17 gprestore:gpadmin:0ee2f5fb02c9:017819-[INFO]:-Post-data metadata restore complete
```

`gprestore` does not attempt to restore global metadata for the Greenplum System by default. If this is required, include the `--with-globals` argument.

By default, `gprestore` uses 1 connection to restore table data and metadata. If you have a large backup set, you can improve performance of the restore by increasing the number of parallel connections with the `--jobs` option. For example:

```
$ gprestore --backup-dir /home/gpadmin/backups/ --timestamp 20171103153156 --create-db --jobs 8
```

Test the number of parallel connections with your backup set to determine the ideal number for fast data recovery.

**Note:** You cannot perform a parallel restore operation with `gprestore` if the backup combined table backups into a single file per segment with the `gpbackup` option `--single-data-file`.

Restoring a materialized view does not restore materialized view data. Only the materialized view definition is restored. To populate the materialized view with data, use `REFRESH MATERIALIZED VIEW`. The tables that are referenced by the materialized view definition must be available when you refresh the materialized view. The `gprestore` log file lists the materialized views that were restored and the `REFRESH MATERIALIZED VIEW` commands that are used to populate the materialized views with data.

### <a id="report_files"></a>Report Files 

When performing a backup or restore operation, `gpbackup` and `gprestore` generate a report file. When email notification is configured, the email sent contains the contents of the report file. For information about email notification, see [Configuring Email Notifications](#topic_qwd_d5d_tbb).

The report file is placed in the Greenplum Database master backup directory. The report file name contains the timestamp of the operation. These are the formats of the `gpbackup` and `gprestore` report file names.

```
gpbackup_<backup_timestamp>_report
gprestore_<backup_timestamp>_<restore_timesamp>_report
```

For these example report file names, `20180213114446` is the timestamp of the backup and `20180213115426` is the timestamp of the restore operation.

```
gpbackup_20180213114446_report
gprestore_20180213114446_20180213115426_report
```

This backup directory on a Greenplum Database master host contains both a `gpbackup` and `gprestore` report file.

```
$ ls -l /gpmaster/seg-1/backups/20180213/20180213114446
total 36
-r--r--r--. 1 gpadmin gpadmin  295 Feb 13 11:44 gpbackup_20180213114446_config.yaml
-r--r--r--. 1 gpadmin gpadmin 1855 Feb 13 11:44 gpbackup_20180213114446_metadata.sql
-r--r--r--. 1 gpadmin gpadmin 1402 Feb 13 11:44 gpbackup_20180213114446_report
-r--r--r--. 1 gpadmin gpadmin 2199 Feb 13 11:44 gpbackup_20180213114446_toc.yaml
-r--r--r--. 1 gpadmin gpadmin  404 Feb 13 11:54 gprestore_20180213114446_20180213115426_report
```

The contents of the report files are similar. This is an example of the contents of a `gprestore` report file.

```
Greenplum Database Restore Report

Timestamp Key: 20180213114446
GPDB Version: 5.4.1+dev.8.g9f83645 build commit:9f836456b00f855959d52749d5790ed1c6efc042
gprestore Version: 1.0.0-alpha.3+dev.73.g0406681

Database Name: test
Command Line: gprestore --timestamp 20180213114446 --with-globals --createdb

Start Time: 2018-02-13 11:54:26
End Time: 2018-02-13 11:54:31
Duration: 0:00:05

Restore Status: Success
```

### <a id="hist"></a>History File 

When performing a backup operation, `gpbackup` appends backup information in the gpbackup history file, `gpbackup_history.yaml`, in the Greenplum Database master data directory. The file contains the backup timestamp, information about the backup options, and backup set information for incremental backups. This file is not backed up by `gpbackup`.

`gpbackup` uses the information in the file to find a matching backup for an incremental backup when you run `gpbackup` with the `--incremental` option and do not specify the `--from-timesamp` option to indicate the backup that you want to use as the latest backup in the incremental backup set. For information about incremental backups, see [Creating and Using Incremental Backups with gpbackup and gprestore](backup-gpbackup-incremental.html).

### <a id="return"></a>Return Codes 

One of these codes is returned after `gpbackup` or `gprestore` completes.

-   **0** – Backup or restore completed with no problems
-   **1** – Backup or restore completed with non-fatal errors. See log file for more information.
-   **2** – Backup or restore failed with a fatal error. See log file for more information.

**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

## <a id="topic_et4_b5d_tbb"></a>Filtering the Contents of a Backup or Restore 

`gpbackup` backs up all schemas and tables in the specified database, unless you exclude or include individual schema or table objects with schema level or table level filter options.

The schema level options are `--include-schema`, `--include-schema-file`, or `--exclude-schema`, `--exclude-schema-file` command-line options to `gpbackup`. For example, if the "demo" database includes only two schemas, "wikipedia" and "twitter," both of the following commands back up only the "wikipedia" schema:

```
$ gpbackup --dbname demo --include-schema wikipedia
$ gpbackup --dbname demo --exclude-schema twitter
```

You can include multiple `--include-schema` options in a `gpbackup` *or* multiple `--exclude-schema` options. For example:

```
$ gpbackup --dbname demo --include-schema wikipedia --include-schema twitter
```

If you have a large number of schemas, you can list the schemas in a text file and specify the file with the `--include-schema-file` or `--exclude-schema-file` options in a `gpbackup` command. Each line in the file must define a single schema, and the file cannot contain trailing lines. For example, this command uses a file in the `gpadmin` home directory to include a set of schemas.

```
gpbackup --dbname demo --include-schema-file /users/home/gpadmin/backup-schemas
```

To filter the individual tables that are included in a backup set, or excluded from a backup set, specify individual tables with the `--include-table` option or the `--exclude-table` option. The table must be schema qualified, `<schema-name>.<table-name>`. The individual table filtering options can be specified multiple times. However, `--include-table` and `--exclude-table` cannot both be used in the same command.

You can create a list of qualified table names in a text file. When listing tables in a file, each line in the text file must define a single table using the format `<schema-name>.<table-name>`. The file must not include trailing lines. For example:

```
wikipedia.articles
twitter.message
```

If a table or schema name uses any character other than a lowercase letter, number, or an underscore character, then you must include that name in double quotes. For example:

```
beer."IPA"
"Wine".riesling
"Wine"."sauvignon blanc"
water.tonic
```

After creating the file, you can use it either to include or exclude tables with the `gpbackup` options `--include-table-file` or `--exclude-table-file`. For example:

```
$ gpbackup --dbname demo --include-table-file /home/gpadmin/table-list.txt
```

You can combine `-include schema` with `--exclude-table` or `--exclude-table-file` for a backup. This example uses `--include-schema` with `--exclude-table` to back up a schema except for a single table.

```
$ gpbackup --dbname demo --include-schema mydata --exclude-table mydata.addresses
```

You cannot combine `--include-schema` with `--include-table` or `--include-table-file`, and you cannot combine `--exclude-schema` with any table filtering option such as `--exclude-table` or `--include-table`.

When you use `--include-table` or `--include-table-file` dependent objects are not automatically backed up or restored, you must explicitly specify the dependent objects that are required. For example, if you back up or restore a view or materialized view, you must also specify the tables that the view or the materialized view uses. If you backup or restore a table that uses a sequence, you must also specify the sequence.

### <a id="section_ddf_gyn_5bb"></a>Filtering by Leaf Partition 

By default, `gpbackup` creates one file for each table on a segment. You can specify the `--leaf-partition-data` option to create one data file per leaf partition of a partitioned table, instead of a single file. You can also filter backups to specific leaf partitions by listing the leaf partition names in a text file to include. For example, consider a table that was created using the statement:

```
demo=# CREATE TABLE sales (id int, date date, amt decimal(10,2))
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
( PARTITION Jan17 START (date '2017-01-01') INCLUSIVE ,
PARTITION Feb17 START (date '2017-02-01') INCLUSIVE ,
PARTITION Mar17 START (date '2017-03-01') INCLUSIVE ,
PARTITION Apr17 START (date '2017-04-01') INCLUSIVE ,
PARTITION May17 START (date '2017-05-01') INCLUSIVE ,
PARTITION Jun17 START (date '2017-06-01') INCLUSIVE ,
PARTITION Jul17 START (date '2017-07-01') INCLUSIVE ,
PARTITION Aug17 START (date '2017-08-01') INCLUSIVE ,
PARTITION Sep17 START (date '2017-09-01') INCLUSIVE ,
PARTITION Oct17 START (date '2017-10-01') INCLUSIVE ,
PARTITION Nov17 START (date '2017-11-01') INCLUSIVE ,
PARTITION Dec17 START (date '2017-12-01') INCLUSIVE
END (date '2018-01-01') EXCLUSIVE );
NOTICE:  CREATE TABLE will create partition "sales_1_prt_jan17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_feb17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_mar17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_apr17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_may17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_jun17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_jul17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_aug17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_sep17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_oct17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_nov17" for table "sales"
NOTICE:  CREATE TABLE will create partition "sales_1_prt_dec17" for table "sales"
CREATE TABLE
```

To back up only data for the last quarter of the year, first create a text file that lists those leaf partition names instead of the full table name:

```
public.sales_1_prt_oct17
public.sales_1_prt_nov17
public.sales_1_prt_dec17 
```

Then specify the file with the `--include-table-file` option to generate one data file per leaf partition:

```
$ gpbackup --dbname demo --include-table-file last-quarter.txt --leaf-partition-data
```

When you specify `--leaf-partition-data`, `gpbackup` generates one data file per leaf partition when backing up a partitioned table. For example, this command generates one data file for each leaf partition:

```
$ gpbackup --dbname demo --include-table public.sales --leaf-partition-data
```

When leaf partitions are backed up, the leaf partition data is backed up along with the metadata for the entire partitioned table.

**Note:** You cannot use the `--exclude-table-file` option with `--leaf-partition-data`. Although you can specify leaf partition names in a file specified with `--exclude-table-file`, `gpbackup` ignores the partition names.

### <a id="filter"></a>Filtering with gprestore 

After creating a backup set with `gpbackup`, you can filter the schemas and tables that you want to restore from the backup set using the `gprestore` `--include-schema` and `--include-table-file` options. These options work in the same way as their `gpbackup` counterparts, but have the following restrictions:

-   The tables that you attempt to restore must not already exist in the database.
-   If you attempt to restore a schema or table that does not exist in the backup set, the `gprestore` does not run.
-   If you use the `--include-schema` option, `gprestore` cannot restore objects that have dependencies on multiple schemas.
-   If you use the `--include-table-file` option, `gprestore` does not create roles or set the owner of the tables. The utility restores table indexes and rules. Triggers are also restored but are not supported in Greenplum Database.
-   The file that you specify with `--include-table-file` cannot include a leaf partition name, as it can when you specify this option with `gpbackup`. If you specified leaf partitions in the backup set, specify the partitioned table to restore the leaf partition data.

    When restoring a backup set that contains data from some leaf partitions of a partitioned table, the partitioned table is restored along with the data for the leaf partitions. For example, you create a backup with the `gpbackup` option `--include-table-file` and the text file lists some leaf partitions of a partitioned table. Restoring the backup creates the partitioned table and restores the data only for the leaf partitions listed in the file.


**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

## <a id="topic_qwd_d5d_tbb"></a>Configuring Email Notifications 

`gpbackup` and `gprestore` can send email notifications after a back up or restore operation completes.

To have `gpbackup` or `gprestore` send out status email notifications, you must place a file named `gp_email_contacts.yaml` in the home directory of the user running `gpbackup` or `gprestore` in the same directory as the utilities \(`$GPHOME/bin`\). A utility issues a message if it cannot locate a `gp_email_contacts.yaml` file in either location. If both locations contain a `.yaml` file, the utility uses the file in user `$HOME`.

The email subject line includes the utility name, timestamp, status, and the name of the Greenplum Database master. This is an example subject line for a `gpbackup` email.

```
gpbackup 20180202133601 on gp-master completed
```

The email contains summary information about the operation including options, duration, and number of objects backed up or restored. For information about the contents of a notification email, see [Report Files](#report_files).

**Note:** The UNIX mail utility must be running on the Greenplum Database host and must be configured to allow the Greenplum superuser \(`gpadmin`\) to send email. Also ensure that the mail program executable is locatable via the `gpadmin` user's `$PATH`.

**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

### <a id="topic_uq5_v5v_scb"></a>gpbackup and gprestore Email File Format 

The `gpbackup` and `gprestore` email notification YAML file `gp_email_contacts.yaml` uses indentation \(spaces\) to determine the document hierarchy and the relationships of the sections to one another. The use of white space is significant. White space should not be used simply for formatting purposes, and tabs should not be used at all.

**Note:** If the `status` parameters are not specified correctly, the utility does not issue a warning. For example, if the `success` parameter is misspelled and is set to `true`, a warning is not issued and an email is not sent to the email address after a successful operation. To ensure email notification is configured correctly, run tests with email notifications configured.

This is the format of the `gp_email_contacts.yaml` YAML file for `gpbackup` email notifications:

```
contacts:
  gpbackup:
  - address: <user>@<domain>
    status:
         success: [true | false]
         success_with_errors: [true | false]
         failure: [true | false]
  gprestore:
  - address: <use>r@<domain>
    status:
         success: [true | false]
         success_with_errors: [true | false]
         failure: [true | false]
```

#### <a id="email"></a>Email YAML File Sections 

**contacts**
:   Required. The section that contains the `gpbackup` and `gprestore` sections. The YAML file can contain a `gpbackup` section, a `gprestore` section, or one of each.

**gpbackup**
:   Optional. Begins the `gpbackup` email section.

**address**
:   Required. At least one email address must be specified. Multiple email `address` parameters can be specified. Each `address` requires a `status` section.

user@domain is a single, valid email address.

**status**
:   Required. Specify when the utility sends an email to the specified email address. The default is to not send email notification.

You specify sending email notifications based on the completion status of a backup or restore operation. At least one of these parameters must be specified and each parameter can appear at most once.

**success**
:   Optional. Specify if an email is sent if the operation completes without errors. If the value is `true`, an email is sent if the operation completes without errors. If the value is `false` \(the default\), an email is not sent.

**success\_with\_errors**
:   Optional. Specify if an email is sent if the operation completes with errors. If the value is `true`, an email is sent if the operation completes with errors. If the value is `false` \(the default\), an email is not sent.

**failure**
:   Optional. Specify if an email is sent if the operation fails. If the value is `true`, an email is sent if the operation fails. If the value is `false` \(the default\), an email is not sent.

**gprestore**
:   Optional. Begins the `gprestore` email section. This section contains the [address](#address_yml) and [status](#status_yml) parameters that are used to send an email notification after a `gprestore` operation. The syntax is the same as the [gpbackup](#gpbackup_yml) section.

#### <a id="exls"></a>Examples 

This example YAML file specifies sending email to email addresses depending on the success or failure of an operation. For a backup operation, an email is sent to a different address depending on the success or failure of the backup operation. For a restore operation, an email is sent to `gpadmin@example.com` only when the operation succeeds or completes with errors.

```
contacts:
  gpbackup:
  - address: gpadmin@example.com
    status:
      success:true
  - address: my_dba@example.com
    status:
      success_with_errors: true
      failure: true
  gprestore:
  - address: gpadmin@example.com
    status:
      success: true
      success_with_errors: true
```

## <a id="topic_xnj_b4c_tbb"></a>Understanding Backup Files 

**Warning:** All `gpbackup` metadata files are created with read-only permissions. Never delete or modify the metadata files for a `gpbackup` backup set. Doing so will render the backup files non-functional.

A complete backup set for `gpbackup` includes multiple metadata files, supporting files, and CSV data files, each designated with the timestamp at which the backup was created.

By default, metadata and supporting files are stored on the Greenplum Database master host in the directory $COORDINATOR\_DATA\_DIRECTORY/backups/YYYYMMDD/YYYYMMDDHHMMSS/. If you specify a custom backup directory, this same file path is created as a subdirectory of the backup directory. The following table describes the names and contents of the metadata and supporting files.

<table class="table frame-all" id="topic_xnj_b4c_tbb__table_nrz_4gj_tbb"><caption><span class="table--title-label">Table 2. </span><span class="title">gpbackup Metadata Files (master)</span></caption><colgroup><col style="width:50%"><col style="width:50%"></colgroup><thead class="thead">
              <tr class="row">
                <th class="entry colsep-1 rowsep-1" id="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__1">File name</th>
                <th class="entry colsep-1 rowsep-1" id="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__2">Description</th>
              </tr>
            </thead><tbody class="tbody">
              <tr class="row">
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__1"><span class="ph filepath">gpbackup_&lt;YYYYMMDDHHMMSS&gt;_metadata.sql</span></td>
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__2">Contains global and database-specific metadata:<ul class="ul" id="topic_xnj_b4c_tbb__ul_l2c_dtv_scb">
                    <li class="li">DDL for objects that are global to the Greenplum Database cluster, and not
                      owned by a specific database within the cluster.</li>
                    <li class="li">DDL for objects in the backed-up database (specified with
                        <code class="ph codeph">--dbname)</code> that must be created <em class="ph i">before</em> to restoring
                      the actual data, and DDL for objects that must be created <em class="ph i">after</em>
                      restoring the data.</li>
                  </ul>Global objects include:<ul class="ul" id="topic_xnj_b4c_tbb__ul_f1b_dhj_tbb">
                    <li class="li">Tablespaces</li>
                    <li class="li">Databases</li>
                    <li class="li">Database-wide configuration parameter settings (GUCs)</li>
                    <li class="li">Resource group definitions</li>
                    <li class="li">Resource queue definitions</li>
                    <li class="li">Roles</li>
                    <li class="li"><code class="ph codeph">GRANT</code> assignments of roles to databases</li>
                  </ul><p class="p"><strong class="ph b">Note:</strong> Global metadata is not restored by default. You must include
                    the <code class="ph codeph">--with-globals</code> option to the <code class="ph codeph">gprestore</code>
                    command to restore global metadata.</p>Database-specific objects that must be
                  created <em class="ph i">before</em> to restoring the actual data include:<ul class="ul" id="topic_xnj_b4c_tbb__ul_vqn_vsv_scb">
                    <li class="li">Session-level configuration parameter settings (GUCs)</li>
                    <li class="li">Schemas</li>
                    <li class="li">Procedural language extensions</li>
                    <li class="li">Types</li>
                    <li class="li">Sequences</li>
                    <li class="li">Functions</li>
                    <li class="li">Tables</li>
                    <li class="li">Protocols</li>
                    <li class="li">Operators and operator classes</li>
                    <li class="li">Conversions</li>
                    <li class="li">Aggregates</li>
                    <li class="li">Casts</li>
                    <li class="li">Views</li>
                    <li class="li">Materialized Views <strong class="ph b">Note:</strong> Materialized view data is not restored, only
                      the definition.</li>
                    <li class="li">Constraints</li>
                  </ul>Database-specific objects that must be created <em class="ph i">after</em> restoring the
                  actual data include:<ul class="ul" id="topic_xnj_b4c_tbb__ul_br4_wsv_scb">
                    <li dir="ltr" class="li">Indexes</li>
                    <li dir="ltr" class="li">Rules</li>
                    <li dir="ltr" class="li">Triggers. (While Greenplum Database does not support triggers, any
                      trigger definitions that are present are backed up and restored.)</li>
                  </ul></td>
              </tr>
              <tr class="row">
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__1"><span class="ph filepath">gpbackup_&lt;YYYYMMDDHHMMSS&gt;_toc.yaml</span></td>
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__2">Contains metadata for locating object DDL in the
                    <span class="ph filepath">_predata.sql</span> and <span class="ph filepath">_postdata.sql</span> files.
                  This file also contains the table names and OIDs used for locating the
                  corresponding table data in CSV data files that are created on each segment. See
                    <a class="xref" href="#topic_xnj_b4c_tbb__section_oys_cpj_tbb">Segment Data Files</a>.</td>
              </tr>
              <tr class="row">
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__1"><span class="ph filepath">gpbackup_&lt;YYYYMMDDHHMMSS&gt;_report</span></td>
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__2">Contains information about the backup operation that is used to populate the
                  email notice (if configured) that is sent after the backup completes. This file
                  contains information such as:<ul class="ul" id="topic_xnj_b4c_tbb__ul_bjr_2kj_tbb">
                    <li class="li">Command-line options that were provided</li>
                    <li class="li">Database that was backed up</li>
                    <li class="li">Database version</li>
                    <li class="li">Backup type</li>
                  </ul>See <a class="xref" href="#topic_qwd_d5d_tbb">Configuring Email Notifications</a>.</td>
              </tr>
              <tr class="row">
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__1"><span class="ph filepath">gpbackup_&lt;YYYYMMDDHHMMSS&gt;_config.yaml</span></td>
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__2">Contains metadata about the execution of the particular backup task,
                  including: <ul class="ul" id="topic_xnj_b4c_tbb__ul_ids_vgj_tbb">
                    <li dir="ltr" class="li"><code class="ph codeph">gpbackup</code> version</li>
                    <li dir="ltr" class="li">Database name</li>
                    <li dir="ltr" class="li">Greenplum Database version</li>
                    <li dir="ltr" class="li">Additional option settings such as
                        <code class="ph codeph">--no-compression</code>, <code class="ph codeph">--compression-level</code>,
                        <code class="ph codeph">--metadata-only</code>, <code class="ph codeph">--data-only</code>, and
                        <code class="ph codeph">--with-stats</code>.</li>
                  </ul></td>
              </tr>
              <tr class="row">
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__1"><span class="ph filepath">gpbackup_history.yaml</span></td>
                <td class="entry colsep-1 rowsep-1" headers="topic_xnj_b4c_tbb__table_nrz_4gj_tbb__entry__2">Contains information about options that were used when creating a backup with
                    <code class="ph codeph">gpbackup</code>, and information about incremental backups.<p class="p">Stored
                    on the Greenplum Database master host in the Greenplum Database master data
                    directory.</p><p class="p">This file is not backed up by <code class="ph codeph">gpbackup</code>.</p><p class="p">
                    For information about incremental backups, see <a class="xref" href="backup-gpbackup-incremental.html">Creating and Using Incremental Backups with gpbackup and gprestore</a>.</p></td>
              </tr>
            </tbody></table>

### <a id="section_oys_cpj_tbb"></a>Segment Data Files 

By default, each segment creates one compressed CSV file for each table that is backed up on the segment. You can optionally specify the `--single-data-file` option to create a single data file on each segment. The files are stored in &lt;seg\_dir\>/backups/YYYYMMDD/YYYYMMDDHHMMSS/.

If you specify a custom backup directory, segment data files are copied to this same file path as a subdirectory of the backup directory. If you include the `--leaf-partition-data` option, `gpbackup` creates one data file for each leaf partition of a partitioned table, instead of just one table for file.

Each data file uses the file name format gpbackup\_&lt;content\_id\>\_&lt;YYYYMMDDHHMMSS\>\_&lt;oid\>.gz where:

-   &lt;content\_id\> is the content ID of the segment.
-   &lt;YYYYMMDDHHMMSS\> is the timestamp of the `gpbackup` operation.
-   &lt;oid\> is the object ID of the table. The metadata file gpbackup\_&lt;YYYYMMDDHHMMSS\>\_toc.yaml references this &lt;oid\> to locate the data for a specific table in a schema.

You can optionally specify the gzip compression level \(from 1-9\) using the `--compression-level` option, or disable compression entirely with `--no-compression`. If you do not specify a compression level, `gpbackup` uses compression level 1 by default.

**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

