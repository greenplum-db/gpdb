# About the Segment Recovery Process 

This topic describes the process for recovering segments, initiated by the `gprecoverseg` management utility. It describes actions `gprecoverseg` performs and how the Greenplum File Replication and FTS \(fault tolerance service\) processes complete the recovery initiated with `gprecoverseg`.

Although `gprecoverseg` is an online operation, there are two brief periods during which all IO is paused. First, when recovery is initiated, IO is suspended while empty data files are created on the mirror. Second, after data files are synchronized, IO is suspended while system files such as transaction logs are copied from the primary to the mirror. The duration of these pauses is affected primarily by the number of file system files that must be created on the mirror and the sizes of the system flat files that must be copied. The system is online and available while database tables are replicated from the primary to the mirror, and any changes made to the database during recovery are replicated directly to the mirror.

To initiate recovery, the administrator runs the `gprecoverseg` utility. `gprecoverseg` prepares segments for recovery and initiates synchronization. When synchronization is complete and the segment status is updated in the system catalog, the segments are recovered. If the recovered segments are not running in their preferred roles, `gprecoverseg -r` can be used to bring the system back into balance.

Without the `-F` option, `gprecoverseg` recovers segments incrementally, copying only the changes since the mirror entered down status. The `-F` option fully recovers mirrors by deleting their data directories and then synchronizing all persistent data files from the primary to the mirror.

You can run `gpstate -e` to view the mirroring status of the segments before and during the recovery process. The primary and mirror segment statuses are updated as the recovery process proceeds.

Consider a single primary-mirror segment pair where the primary is active and the mirror is down. The following table shows the segment status before beginning recovery of the mirror.

| |`preferred_role`|`role`|`mode`|`status`|
|--|----------------|------|------|--------|
|Primary|`p`\(primary\)<br/></br>|`p`\(primary\)<br/></br>|`c`\(change tracking\)<br/></br>|`u`\(up\)|
|Mirror|`m`\(mirror\)|`m`\(mirror\)|`s`\(synchronizing\)|`d`\(down\)|

The segments are in their preferred roles, but the mirror is down. The primary is up and is in change tracking mode because it is unable to send changes to its mirror.

## Segment Recovery Preparation 

The `gprecoverseg` utility prepares the segments for recovery and then exits, allowing the Greenplum file replication processes to copy data from the primary to the mirror.

During the `gprecoverseg` execution the following recovery process steps are completed.

1.  The down segments are identified.
2.  The mirror segment processes are initialized.
3.  For full recovery \(`-aF`\):
    -   The data directories of the down segments are deleted.
    -   A new data directory structure is created.
4.  The segment mode in the `gp_segment_configuration` system table is updated to 'r' \(resynchronization mode\).
5.  The backend performs the following:

    -   Suspends IO—connections to the master are allowed, but reads and writes from the segment being recovered are not allowed.
    -   Scans persistent tables on the primary segment.
    -   For each persistent file object \(`relfilenode` in the `pg_class` system table\), creates a data file on the mirror.
    The greater the number of data files, the longer IO is suspended.

    For incremental recovery, the IO is suspended for a shorter period because only file system objects added \(or dropped\) on the primary after the mirror was marked down need to be created \(or deleted\) on the mirror.

6.  The `gprecoverseg` script completes.

Once `gprecoverseg` has completed, the segments are in the states shown in the following table.

| |`preferred_role`|`role`|`mode`|`status`|
|--|----------------|------|------|--------|
|Primary|`p`\(primary\)|`p`\(primary\)|`r`\(resynchronizing\)|`u`\(up\)|
|Mirror|`m`\(mirror\)|`m`\(mirror\)|`r`\(resynchronizing\)|`u`\(up\)|

## Data File Replication 

Data file resynchronization is performed in the background by file replication processes. Run `gpstate -e` to check the process of resynchronization. The Greenplum system is fully available for workloads while this process completes.

Following are steps in the resynchronization process:

1.  Data copy \(full and incremental recovery\):

    After the file system objects are created, data copy is initiated for the affected segments. The ResyncManager process scans the persistent table system catalogs to find the file objects to be synchronized. ResyncWorker processes sync the file objects from the primary to the mirror.

2.  Any changes or new data created with database transactions during the data copy are mirrored directly to the mirror.
3.  Once data copy has finished synchronizing persistent data files, file replication updates the shared memory state on the current primary segment to 'insync'.

## Flat File Replication 

During this phase, system files in the primary segment's data directory are copied to the segment data directory. IO is suspended while the following flat files are copied from the primary data directory to the segment data directory:

-   `pg_xlog/*`
-   `pg_clog/*`
-   `pg_distributedlog/*`
-   `pg_distributedxidmap/*`
-   `pg_multixact/members`
-   `pg_multixact/offsets`
-   `pg_twophase/*`
-   `global/pg_database`
-   `global/pg_auth`
-   `global/pg_auth_time_constraint`

IOSUSPEND ends after these files are copied.

The next time the fault tolerance server \(ftsprobe\) process on the master wakes, it will set the primary and mirror states to synchronized \(mode=s, state=u\). A distributed query will also trigger the ftsprobe process to update the state.

When all segment recovery and file replication processes are complete, the segment status in the `gp_segment_configuration` system table and `gp_state -e` output is as shown in the following table.

| |`preferred_role`|`role`|`mode`|`status`|
|--|----------------|------|------|--------|
|Primary|`p`\(primary\)|`p`\(primary\)|`s`\(synchronized\)|`u`\(up\)|
|Mirror|`m`\(mirror\)|`m`\(mirror\)|`s`\(synchronized\)|`u`\(up\)|

## Factors Affecting Duration of Segment Recovery 

Following are some factors that can affect the duration of the segment recovery process.

-   The number of database objects, mainly tables and indexes.
-   The number of data files in segment data directories.
-   The types of workloads updating data during resynchronization – both DDL and DML \(insert, update, delete, and truncate\).
-   The size of the data.
-   The size of system files, such as transaction log files, pg\_database, pg\_auth, and pg\_auth\_time\_constraint.

**Parent topic:** [Recovering From Segment Failures](../../highavail/topics/g-recovering-from-segment-failures.html)

