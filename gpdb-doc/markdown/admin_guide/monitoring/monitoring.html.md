---
title: Recommended Monitoring and Maintenance Tasks 
---

This section lists monitoring and maintenance activities recommended to ensure high availability and consistent performance of your Greenplum Database cluster.

The tables in the following sections suggest activities that a Greenplum System Administrator can perform periodically to ensure that all components of the system are operating optimally. Monitoring activities help you to detect and diagnose problems early. Maintenance activities help you to keep the system up-to-date and avoid deteriorating performance, for example, from bloated system tables or diminishing free disk space.

It is not necessary to implement all of these suggestions in every cluster; use the frequency and severity recommendations as a guide to implement measures according to your service requirements.

**Parent topic:**[Managing a Greenplum System](../managing/partII.html)

## <a id="drr_5bg_rp"></a>Database State Monitoring Activities 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|List segments that are currently down. If any rows are returned, this should generate a warning or alert.Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Run the following query in the `postgres` database:```
SELECT * FROM gp_segment_configuration
WHERE status = 'd';
```

|If the query returns any rows, follow these steps to correct the problem:1.  Verify that the hosts with down segments are responsive.
2.  If hosts are OK, check the log files for the primaries and mirrors of thedown segments to discover the root cause of the segments going down.
3.  If no unexpected errors are found, run the `gprecoverseg` utility to bring the segments back online.

|
|Check for segments that are up and not in sync. If rows are returned, this should generate a warning or alert.Recommended frequency: run every 5 to 10 minutes

|Execute the following query in the `postgres` database:

```
SELECT * FROM gp_segment_configuration
WHERE mode = 'n' and status = 'u' and content <> -1;
```

|If the query returns rows then the segment might be in the process of moving from `Not In Sync` to `Synchronized` mode. Use `gpstate -e` to track progress.|
|Check for segments that are not operating in their preferred role but are marked as up and `Synchronized`. If any segments are found, the cluster may not be balanced. If any rows are returned this should generate a warning or alert. Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Execute the following query in the `postgres` database:

```
SELECT * FROM gp_segment_configuration WHERE preferred_role <> role  and status = 'u' and mode = 's';
```

|When the segments are not running in their preferred role, processing might be skewed. Run `gprecoverseg -r` to bring the segments back into their preferred roles.

|
|Run a distributed query to test that it runs on all segments. One row should be returned for each primary segment. Recommended frequency: run every 5 to 10 minutes

Severity: CRITICAL

|Execute the following query in the `postgres` database:

```
SELECT gp_segment_id, count(*)
FROM gp_dist_random('pg_class')
GROUP BY 1;
```

|If this query fails, there is an issue dispatching to some segments in the cluster. This is a rare event. Check the hosts that are not able to be dispatched to ensure there is no hardware or networking issue.

|
|Test the state of master mirroring on Greenplum Database. If the value is not "STREAMING", raise an alert or warning.Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Run the following `psql` command:

```
psql <dbname> -c 'SELECT pid, state FROM pg_stat_replication;'
```

|Check the log file from the master and standby master for errors. If there are no unexpected errors and the machines are up, run the `gpinitstandby` utility to bring the standby online.

|
|Perform a basic check to see if the master is up and functioning.Recommended frequency: run every 5 to 10 minutes

Severity: CRITICAL

|Run the following query in the `postgres` database:

```
SELECT count(*) FROM gp_segment_configuration;
```

|If this query fails, the active master may be down. Try to start the database on the original master if the server is up and running. If that fails, try to activate the standby master as master.

|

## <a id="topic_y4c_4gg_rp"></a>Hardware and Operating System Monitoring 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Check disk space usage on volumes used for Greenplum Database data storage and the OS. Recommended frequency: every 5 to 30 minutes

Severity: CRITICAL

|Set up a disk space check.

-   Set a threshold to raise an alert when a disk reaches a percentage of capacity. The recommended threshold is 75% full.
-   It is not recommended to run the system with capacities approaching 100%.

|Use `VACUUM`/`VACUUM FULL` on user tables to reclaim space occupied by dead rows.|
|Check for errors or dropped packets on the network interfaces.Recommended frequency: hourly

Severity: IMPORTANT

|Set up a network interface checks.|Work with network and OS teams to resolve errors.

|
|Check for RAID errors or degraded RAID performance. Recommended frequency: every 5 minutes

Severity: CRITICAL

|Set up a RAID check.|-   Replace failed disks as soon as possible.
-   Work with system administration team to resolve other RAID or controller errors as soon as possible.

|
|Check for adequate I/O bandwidth and I/O skew.Recommended frequency: when create a cluster or when hardware issues are suspected.

|Run the Greenplum `gpcheckperf` utility.|The cluster may be under-specified if data transfer rates are not similar to the following:

-   2GB per second disk read
-   1 GB per second disk write
-   10 Gigabit per second network read and write

If transfer rates are lower than expected, consult with your data architect regarding performance expectations.

 If the machines on the cluster display an uneven performance profile, work with the system administration team to fix faulty machines.

|

## <a id="topic_gbp_jng_rp"></a>Catalog Monitoring 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Run catalog consistency checks in each database to ensure the catalog on each host in the cluster is consistent and in a good state.You may run this command while the database is up and running.

Recommended frequency: weekly

Severity: IMPORTANT

|Run the Greenplum `gpcheckcat` utility in each database:```
gpcheckcat -O
```

**Note:** With the `-O` option, `gpcheckcat` runs just 10 of its usual 15 tests.

|Run the repair scripts for any issues identified.|
|Check for `pg_class` entries that have no corresponding pg\_`attribute` entry.Recommended frequency: monthly

Severity: IMPORTANT

|During a downtime, with no users on the system, run the Greenplum `gpcheckcat` utility in each database:```
gpcheckcat -R pgclass
```

|Run the repair scripts for any issues identified.|
|Check for leaked temporary schema and missing schema definition.Recommended frequency: monthly

Severity: IMPORTANT

|During a downtime, with no users on the system, run the Greenplum `gpcheckcat` utility in each database:```
gpcheckcat -R namespace
```

|Run the repair scripts for any issues identified.|
|Check constraints on randomly distributed tables.Recommended frequency: monthly

Severity: IMPORTANT

|During a downtime, with no users on the system, run the Greenplum `gpcheckcat` utility in each database:```
gpcheckcat -R distribution_policy
```

|Run the repair scripts for any issues identified.|
|Check for dependencies on non-existent objects.Recommended frequency: monthly

Severity: IMPORTANT

|During a downtime, with no users on the system, run the Greenplum `gpcheckcat` utility in each database:```
gpcheckcat -R dependency
```

|Run the repair scripts for any issues identified.|

## <a id="maintentenance_check_scripts"></a>Data Maintenance 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Check for missing statistics on tables.|Check the `gp_stats_missing` view in each database:```
SELECT * FROM gp_toolkit.gp_stats_missing;
```

|Run `ANALYZE` on tables that are missing statistics.|
|Check for tables that have bloat \(dead space\) in data files that cannot be recovered by a regular `VACUUM` command. Recommended frequency: weekly or monthly

Severity: WARNING

|Check the `gp_bloat_diag` view in each database: ```
SELECT * FROM gp_toolkit.gp_bloat_diag;
```

|`VACUUM FULL` acquires an `ACCESS EXCLUSIVE` lock on tables. Run `VACUUM FULL` during a time when users and applications do not require access to the tables, such as during a time of low activity, or during a maintenance window.|

## <a id="topic_dld_23h_rp"></a>Database Maintenance 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Reclaim space occupied by deleted rows in the heap tables so that the space they occupy can be reused.Recommended frequency: daily

Severity: CRITICAL

|Vacuum user tables:```
VACUUM <table>;
```

|Vacuum updated tables regularly to prevent bloating.|
|Update table statistics. Recommended frequency: after loading data and before executing queries

Severity: CRITICAL

|Analyze user tables. You can use the `analyzedb` management utility:```
analyzedb -d <database> -a
```

|Analyze updated tables regularly so that the optimizer can produce efficient query execution plans.|
|Backup the database data.Recommended frequency: daily, or as required by your backup plan

Severity: CRITICAL

|Run the `gpbackup` utility to create a backup of the master and segment databases in parallel.|Best practice is to have a current backup ready in case the database must be restored.|
|Vacuum, reindex, and analyze system catalogs to maintain an efficient catalog.Recommended frequency: weekly, or more often if database objects are created and dropped frequently

|1.  `VACUUM` the system tables in each database.
2.  Run `REINDEX SYSTEM` in each database, or use the `reindexdb` command-line utility with the `-s` option:

    ```
reindexdb -s <database>
    ```

3.  `ANALYZE` each of the system tables:

    ```
analyzedb -s pg_catalog -d <database>
    ```


|The optimizer retrieves information from the system tables to create query plans. If system tables and indexes are allowed to become bloated over time, scanning the system tables increases query execution time. It is important to run `ANALYZE` after reindexing, because `REINDEX` leaves indexes with no statistics.|

## <a id="topic_idx_smh_rp"></a>Patching and Upgrading 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Ensure any bug fixes or enhancements are applied to the kernel.Recommended frequency: at least every 6 months

Severity: IMPORTANT

|Follow the vendor's instructions to update the Linux kernel.|Keep the kernel current to include bug fixes and security fixes, and to avoid difficult future upgrades.|
|Install Greenplum Database minor releases, for example 5.0.*x*.Recommended frequency: quarterly

Severity: IMPORTANT

|Follow upgrade instructions in the Greenplum Database *Release Notes*. Always upgrade to the latest in the series.|Keep the Greenplum Database software current to incorporate bug fixes, performance enhancements, and feature enhancements into your Greenplum cluster.|

