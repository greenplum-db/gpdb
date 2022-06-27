# Recommended Monitoring and Maintenance Tasks 

This section lists monitoring and maintenance activities recommended to ensure high availability and consistent performance of your Greenplum Database cluster.

The tables in the following sections suggest activities that a Greenplum System Administrator can perform periodically to ensure that all components of the system are operating optimally. Monitoring activities help you to detect and diagnose problems early. Maintenance activities help you to keep the system up-to-date and avoid deteriorating performance, for example, from bloated system tables or diminishing free disk space.

It is not necessary to implement all of these suggestions in every cluster; use the frequency and severity recommendations as a guide to implement measures according to your service requirements.

**Parent topic:**[Managing a Greenplum System](../managing/partII.html)

## Database State Monitoring Activities 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|List segments that are currently down. If any rows are returned, this should generate a warning or alert.Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Run the following query in the `postgres` database:```
SELECT * FROM gp_segment_configuration
WHERE status <> 'u';
```

|If the query returns any rows, follow these steps to correct the problem:1.  Verify that the hosts with down segments are responsive.
2.  If hosts are OK, check the pg\_log files for the primaries and mirrors of thedown segments to discover the root cause of the segments going down.
3.  If no unexpected errors are found, run the `gprecoverseg` utility to bring the segments back online.

|
|Check for segments that are currently in change tracking mode. If any rows are returned, this should generate a warning or alert.Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Execute the following query in the `postgres` database:

```
SELECT * FROM gp_segment_configuration
WHERE mode = 'c';
```

|If the query returns any rows, follow these steps to correct the problem:

1.  Verify that hosts with down segments are responsive.
2.  If hosts are OK, check the pg\_log files for the primaries and mirrors of the down segments to determine the root cause of the segments going down.
3.  If no unexpected errors are found, run the `gprecoverseg` utility to bring the segments back online.

|
|Check for segments that are currently re-syncing. If rows are returned, this should generate a warning or alert.Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Execute the following query in the `postgres` database:

```
SELECT * FROM gp_segment_configuration
WHERE mode = 'r';
```

|When this query returns rows, it implies that the segments are in the process of being re-synched. If the state does not change from 'r' to 's', then check the pg\_log files from the primaries and mirrors of the affected segments for errors.

|
|Check for segments that are not operating in their optimal role. If any segments are found, the cluster may not be balanced. If any rows are returned this should generate a warning or alert. Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Execute the following query in the `postgres` database:

```
SELECT * FROM gp_segment_configuration
WHERE preferred_role <> role;
```

|When the segments are not running in their preferred role, hosts have uneven numbers of primary segments on each host, implying that processing is skewed. Wait for a potential window and restart the database to bring the segments into their preferred roles.

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
|Test the state of master mirroring on a Greenplum Database4.2 or earlier cluster. If the value is "Not Synchronized", raise an alert or warning.Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Execute the following query in the `postgres` database:

```
SELECT summary_state
FROM gp_master_mirroring;
```

|Check the pg\_log from the master and standby master for errors. If there are no unexpected errors and the machines are up, run the `gpinitstandby` utility to bring the standby online. This requires a database restart on GPDB 4.2 and earlier.

|
|Test the state of master mirroring on Greenplum Database. If the value is not "STREAMING", raise an alert or warning.Recommended frequency: run every 5 to 10 minutes

Severity: IMPORTANT

|Run the following `psql` command:

```
psql <dbname> -c 'SELECT procpid, state FROM pg_stat_replication;'
```

|Check the pg\_log file from the master and standby master for errors. If there are no unexpected errors and the machines are up, run the `gpinitstandby` utility to bring the standby online.

|
|Perform a basic check to see if the master is up and functioning.Recommended frequency: run every 5 to 10 minutes

Severity: CRITICAL

|Run the following query in the `postgres` database:

```
SELECT count(*) FROM gp_segment_configuration;
```

|If this query fails the active master may be down. Try again several times and then inspect the active master manually. If the active master is down, reboot or power cycle the active master to ensure no processes remain on the active master and then trigger the activation of the standby master.

|

## Database Alert Log Monitoring 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Check for FATAL and ERROR log messages from the system.Recommended frequency: run every 15 minutes

Severity: WARNING

*This activity and the next are two methods for monitoring messages in the log\_alert\_history table. It is only necessary to set up one or the other.*

|Run the following query in the `gpperfmon` database:

```
SELECT * FROM log_alert_history
WHERE logseverity in ('FATAL', 'ERROR')
   AND logtime > (now() - interval '15 minutes');
```

|Send an alert to the DBA to analyze the alert. You may want to add additional filters to the query to ignore certain messages of low interest.|
|Set up server configuration parameters to sendSNMP or email alerts.Recommended frequency: N/A. Alerts are generated by the system.

Severity: WARNING

*This activity and the previous are two methods for monitoring messages in the log\_alert\_history table. It is only necessary to set up one or the other.*

|Enable server configuration parameters to send alerts viaSNMP or email:

-   `gp_email_smtp_server`
-   `gp_email_smtp_userid`
-   `gp_email_smtp_password`or `gp_snmp_monitor_address`
-   `gp_snmp_community`
-   `gp_snmp_use_inform_or_trap`

|DBA takes action based on the nature of the alert.

|

## Hardware and Operating System Monitoring 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Underlying platform check for maintenance required or system down of the hardware.Recommended frequency: real-time, if possible, or every 15 minutes

Severity: CRITICAL

|Set upSNMP or other system check for hardware and OS errors.|If required, remove a machine from the Greenplum cluster to resolve hardware and OS issues, then, after add it back to the cluster and run `gprecoverseg`.|
|Check disk space usage on volumes used for Greenplum Database data storage and the OS. Recommended frequency: every 5 to 30 minutes

Severity: CRITICAL

|Set up a disk space check.

-   Set a threshold to raise an alert when a disk reaches a percentage of capacity. The recommended threshold is 75% full.
-   It is not recommended to run the system with capacities approaching 100%.

|Free space on the system by removing some data or files.|
|Check for errors or dropped packets on the network interfaces.Recommended frequency: hourly

Severity: IMPORTANT

|Set up a network interface checks.|Work with network and OS teams to resolve errors.

|
|Check for RAID errors or degraded RAID performance. Recommended frequency: every 5 minutes

Severity: CRITICAL

|Set up a RAID check.|-   Replace failed disks as soon as possible.
-   Work with system administration team to resolve other RAID or controller errors as soon as possible.

|
|Run the Greenplum `gpcheck` utility to test that the configuration of the cluster complies with current recommendations.Recommended frequency: when creating a cluster or adding new machines to the cluster

Severity: IMPORTANT

|Run `gpcheck`.|Work with system administration team to update configuration according to the recommendations made by the `gpcheck` utility.

|
|Check for adequate I/O bandwidth and I/O skew.Recommended frequency: when create a cluster or when hardware issues are suspected.

|Run the Greenplum `gpcheckperf` utility.|The cluster may be under-specified if data transfer rates are not similar to the following:

-   2GB per second disk read
-   1 GB per second disk write
-   10 Gigabit per second network read and write

If transfer rates are lower than expected, consult with your data architect regarding performance expectations.

 If the machines on the cluster display an uneven performance profile, work with the system administration team to fix faulty machines.

|

## Catalog Monitoring 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Run catalog consistency checks to ensure the catalog on each host in the cluster is consistent and in a good state.Recommended frequency: weekly

Severity: IMPORTANT

|Run the Greenplum `gpcheckcat` utility in each database:```
gpcheckcat -O
```

|Run repair scripts for any issues detected.|
|Run a persistent table catalog check.Recommended frequency: monthly

Severity: CRITICAL

|During a downtime, with no users on the system, run the Greenplum `gpcheckcat` utility in each database:```
gpcheckcat -R persistent
```

|Run repair scripts for any issues detected.|
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

## Data Maintenance 

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

|Execute a `VACUUM FULL` statement at a time when users are not accessing the table to remove bloat and compact the data.|

## Database Maintenance 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Mark deleted rows in heap tables so that the space they occupy can be reused.Recommended frequency: daily

Severity: CRITICAL

|Vacuum user tables:```
VACUUM <<table>>;
```

|Vacuum updated tables regularly to prevent bloating.|
|Update table statistics. Recommended frequency: after loading data and before executing queries

Severity: CRITICAL

|Analyze user tables. You can use the `analyzedb` management utility:```
analyzedb -d <<database>> -a
```

|Analyze updated tables regularly so that the optimizer can produce efficient query execution plans.|
|Backup the database data.Recommended frequency: daily, or as required by your backup plan

Severity: CRITICAL

|Run the `gpcrondump` utility to create a backup of the master and segment databases in parallel.|Best practice is to have a current backup ready in case the database must be restored.|
|Vacuum, reindex, and analyze system catalogs to maintain an efficient catalog.Recommended frequency: weekly, or more often if database objects are created and dropped frequently

**Note:** Starting in Greenplum 5.x, `VACUUM` is supported with persistent table system catalogs, and is required to manage free space.

|1.  `VACUUM` the system tables in each database.
2.  Run `REINDEX SYSTEM` in each database, or use the `reindexdb` command-line utility with the `-s` option:

    ```
reindexdb -s <<database>>
    ```

3.  `ANALYZE` each of the system tables:

    ```
analyzedb -s pg_catalog -d <<database>>
    ```


|The optimizer retrieves information from the system tables to create query plans. If system tables and indexes are allowed to become bloated over time, scanning the system tables increases query execution time. It is important to run `ANALYZE` after reindexing, because `REINDEX` leaves indexes with no statistics.|

## Patching and Upgrading 

|Activity|Procedure|Corrective Actions|
|--------|---------|------------------|
|Ensure any bug fixes or enhancements are applied to the kernel.Recommended frequency: at least every 6 months

Severity: IMPORTANT

|Follow the vendor's instructions to update the Linux kernel.|Keep the kernel current to include bug fixes and security fixes, and to avoid difficult future upgrades.|
|Install Greenplum Database minor releases, for example 5.0.*x*.Recommended frequency: quarterly

Severity: IMPORTANT

|Follow upgrade instructions in the Greenplum Database *Release Notes*. Always upgrade to the latest in the series.|Keep the Greenplum Database software current to incorporate bug fixes, performance enhancements, and feature enhancements into your Greenplum cluster.|

