# gpcheckcat 

The `gpcheckcat` utility tests Greenplum Database catalog tables for inconsistencies.

The utility is in `$GPHOME/bin/lib`.

## <a id="section2"></a>Synopsis 

```
gpcheckcat [ <options<] [ <dbname>] 

  Options:
     -g <dir>
     -p <port>
     -s <test_name | 'test_name1, test_name2 [, ...]'>  
     -P <password>
     -U <user_name>
     -S {none | only}
     -O
     -R <test_name | 'test_name1, test_name2 [, ...]'>
     -C <catalog_name>
     -B <parallel_processes>
     -v
     -A

gpcheckcat  -l 

gpcheckcat -? | --help 

```

## <a id="section3"></a>Description 

The `gpcheckcat` utility runs multiple tests that check for database catalog inconsistencies. Some of the tests cannot be run concurrently with other workload statements or the results will not be usable. Restart the database in restricted mode when running `gpcheckcat`, otherwise `gpcheckcat` might report inconsistencies due to ongoing database operations rather than the actual number of inconsistencies. If you run `gpcheckcat` without stopping database activity, run it with `-O` option.

**Note:** Any time you run the utility, it checks for and deletes orphaned, temporary database schemas \(temporary schemas without a session ID\) in the specified databases. The utility displays the results of the orphaned, temporary schema check on the command line and also logs the results.

Catalog inconsistencies are inconsistencies that occur between Greenplum Database system tables. In general, there are three types of inconsistencies:

-   Inconsistencies in system tables at the segment level. For example, an inconsistency between a system table that contains table data and a system table that contains column data. As another, a system table that contains duplicates in a column that should to be unique.

-   Inconsistencies between same system table across segments. For example, a system table is missing row on one segment, but other segments have this row. As another example, the values of specific row column data are different across segments, such as table owner or table access privileges.
-   Inconsistency between a catalog table and the filesystem. For example, a file exists in database directory, but there is no entry for it in the pg\_class table.

## <a id="section4"></a>Options 

-A
:   Run `gpcheckcat` on all databases in the Greenplum Database installation.

-B <parallel\_processes\>
:   The number of processes to run in parallel.

:   The `gpcheckcat` utility attempts to determine the number of simultaneous processes \(the batch size\) to use. The utility assumes it can use a buffer with a minimum of 20MB for each process. The maximum number of parallel processes is the number of Greenplum Database segment instances. The utility displays the number of parallel processes that it uses when it starts checking the catalog.
    **Note:** The utility might run out of memory if the number of errors returned exceeds the buffer size. If an out of memory error occurs, you can lower the batch size with the `-B` option. For example, if the utility displays a batch size of 936 and runs out of memory, you can specify `-B 468` to run 468 processes in parallel.

-C catalog\_table
:   Run cross consistency, foreign key, and ACL tests for the specified catalog table.

-g data\_directory
:   Generate SQL scripts to fix catalog inconsistencies. The scripts are placed in data\_directory.

-l
:   List the `gpcheckcat` tests.

-O
:   Run only the `gpcheckcat` tests that can be run in online \(not restricted\) mode.

-p port
:   This option specifies the port that is used by the Greenplum Database.

-P password
:   The password of the user connecting to Greenplum Database.

-R test\_name \| 'test\_name1,test\_name2 \[, ...\]'
:   Specify one or more tests to run. Specify multiple tests as a comma-delimited list of test names enclosed in quotes.

:   Some tests can be run only when Greenplum Database is in restricted mode.

:   These are the tests that can be performed:

    `acl` - Cross consistency check for access control privileges

    `aoseg_table` - Check that the vertical partition information \(vpinfo\) on segment instances is consistent with `pg_attribute` \(checks only append-optimized, column storage tables in the database\)

    `duplicate` - Check for duplicate entries

    `foreign_key` - Check foreign keys

    `inconsistent` - Cross consistency check for master segment inconsistency

    `missing_extraneous` - Cross consistency check for missing or extraneous entries

    `owner` - Check table ownership that is inconsistent with the master database

    `orphaned_toast_tables` - Check for orphaned TOAST tables.

    **Note:** There are several ways a TOAST table can become orphaned where a repair script cannot be generated and a manual catalog change is required. One way is if the `reltoastrelid` entry in *pg\_class* points to an incorrect TOAST table \(a TOAST table mismatch\). Another way is if both the `reltoastrelid` in *pg\_class* is missing and the `pg_depend` entry is missing \(a double orphan TOAST table\). If a manual catalog change is needed, `gpcheckcat` will display detailed steps you can follow to update the catalog. Contact VMware Support if you need help with the catalog change.

    `part_integrity` - Check *pg\_partition* branch integrity, partition with OIDs, partition distribution policy

    `part_constraint` - Check constraints on partitioned tables

    `unique_index_violation` - Check tables that have columns with the unique index constraint for duplicate entries

    `dependency` - Check for dependency on non-existent objects \(restricted mode only\)

    `distribution_policy` - Check constraints on randomly distributed tables \(restricted mode only\)

    `namespace` - Check for schemas with a missing schema definition \(restricted mode only\)

    `pgclass` - Check *pg\_class* entry that does not have any corresponding *pg\_attribute* entry \(restricted mode only\)

-s `test_name | 'test_name1, test_name2 [, ...]'`
:   Specify one ore more tests to skip. Specify multiple tests as a comma-delimited list of test names enclosed in quotes.

-S \{none \| only\}
:   Specify this option to control the testing of catalog tables that are shared across all databases in the Greenplum Database installation, such as *pg\_database*.

:   The value `none` deactivates testing of shared catalog tables. The value `only` tests only the shared catalog tables.

-U user\_name
:   The user connecting to Greenplum Database.

-? \| --help
:   Displays the online help.

-v \(verbose\)
:   Displays detailed information about the tests that are performed.

## <a id="notes"></a>Notes 

The utility identifies tables with missing attributes and displays them in various locations in the output and in a non-standardized format. The utility also displays a summary list of tables with missing attributes in the format `<database>. <schema>. <table>. <segment_id>` after the output information is displayed.

If `gpcheckcat` detects inconsistent OID \(Object ID\) information, it generates one or more verification files that contain an SQL query. You can run the SQL query to see details about the OID inconsistencies and investigate the inconsistencies. The files are generated in the directory where `gpcheckcat` is invoked.

This is the format of the file:

```
gpcheckcat.verify.dbname.catalog\_table\_name.test\_name.TIMESTAMP.sql
```

This is an example verification filename created by `gpcheckcat` when it detects inconsistent OID \(Object ID\) information in the catalog table *pg\_type* in the database `mydb`:

```
gpcheckcat.verify.mydb.pg_type.missing_extraneous.20150420102715.sql
```

This is an example query from a verification file:

```
SELECT *
  FROM (
       SELECT relname, oid FROM pg_class WHERE reltype 
         IN (1305822,1301043,1301069,1301095)
       UNION ALL
       SELECT relname, oid FROM gp_dist_random('pg_class') WHERE reltype 
         IN (1305822,1301043,1301069,1301095)
       ) alltyprelids
  GROUP BY relname, oid ORDER BY count(*) desc ;
```

