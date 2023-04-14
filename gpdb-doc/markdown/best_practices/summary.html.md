---
title: Best Practices Summary 
---

A summary of best practices for Greenplum Database.

## <a id="datmod"></a>Data Model 

Greenplum Database is an analytical MPP shared-nothing database. This model is significantly different from a highly normalized/transactional SMP database. Because of this, the following best practices are recommended.

-   Greenplum Database performs best with a denormalized schema design suited for MPP analytical processing for example, Star or Snowflake schema, with large fact tables and smaller dimension tables.
-   Use the same data types for columns used in joins between tables.

See [Schema Design](schema.html).

## <a id="heapapp"></a>Heap vs. Append-Optimized Storage 

-   Use heap storage for tables and partitions that will receive iterative batch and singleton `UPDATE`, `DELETE`, and `INSERT` operations.
-   Use heap storage for tables and partitions that will receive concurrent `UPDATE`, `DELETE`, and `INSERT` operations.
-   Use append-optimized storage for tables and partitions that are updated infrequently after the initial load and have subsequent inserts only performed in large batch operations.
-   Avoid performing singleton `INSERT`, `UPDATE`, or `DELETE` operations on append-optimized tables.
-   Avoid performing concurrent batch `UPDATE` or `DELETE` operations on append-optimized tables. Concurrent batch `INSERT` operations are acceptable.

See [Heap Storage or Append-Optimized Storage](schema.html#heap_vs_ao).

## <a id="rowcol"></a>Row vs. Column Oriented Storage 

-   Use row-oriented storage for workloads with iterative transactions where updates are required and frequent inserts are performed.
-   Use row-oriented storage when selects against the table are wide.
-   Use row-oriented storage for general purpose or mixed workloads.
-   Use column-oriented storage where selects are narrow and aggregations of data are computed over a small number of columns.
-   Use column-oriented storage for tables that have single columns that are regularly updated without modifying other columns in the row.

See [Row or Column Orientation](schema.html#row_vs_column).

## <a id="comp"></a>Compression 

-   Use compression on large append-optimized and partitioned tables to improve I/O across the system.
-   Set the column compression settings at the level where the data resides.
-   Balance higher levels of compression with the time and CPU cycles needed to compress and uncompress data.

See [Compression](schema.html).

## <a id="dist"></a>Distributions 

-   Explicitly define a column or random distribution for all tables. Do not use the default.
-   Use a single column that will distribute data across all segments evenly.
-   Do not distribute on columns that will be used in the `WHERE` clause of a query.
-   Do not distribute on dates or timestamps.
-   Never distribute and partition tables on the same column.
-   Achieve local joins to significantly improve performance by distributing on the same column for large tables commonly joined together.
-   To ensure there is no data skew, validate that data is evenly distributed after the initial load and after incremental loads.

See [Distributions](schema.html).

## <a id="rqman"></a>Resource Queue Memory Management 

-   Set `vm.overcommit_memory` to 2.
-   Do not configure the OS to use huge pages.
-   Use `gp_vmem_protect_limit` to set the maximum memory that the instance can allocate for *all* work being done in each segment database.
-   You can use `gp_vmem_protect_limit` by calculating:
    -   `gp_vmem` – the total memory available to Greenplum Database

        -   If the total system memory is less than 256 GB, use this formula:

            ```
            gp_vmem = ((SWAP + RAM) – (7.5GB + 0.05 * RAM)) / 1.7
            ```

        -   If the total system memory is equal to or greater than 256 GB, use this formula:

            ```
            gp_vmem = ((SWAP + RAM) – (7.5GB + 0.05 * RAM)) / 1.17
            ```

        where `SWAP` is the host's swap space in GB, and `RAM` is the host's RAM in GB.

    -   `max_acting_primary_segments` – the maximum number of primary segments that could be running on a host when mirror segments are activated due to a host or segment failure.
    -   `gp_vmem_protect_limit`

        ```
        gp_vmem_protect_limit = gp_vmem / acting_primary_segments
        ```

        Convert to MB to set the value of the configuration parameter.

-   In a scenario where a large number of workfiles are generated calculate the `gp_vmem` factor with this formula to account for the workfiles.
    -   If the total system memory is less than 256 GB:

        ```
        gp_vmem = ((SWAP + RAM) – (7.5GB + 0.05 * RAM - (300KB *
              total_#_workfiles))) / 1.7
        ```

    -   If the total system memory is equal to or greater than 256 GB:

        ```
        gp_vmem = ((SWAP + RAM) – (7.5GB + 0.05 * RAM - (300KB *
              total_#_workfiles))) / 1.17
        ```

-   Never set `gp_vmem_protect_limit` too high or larger than the physical RAM on the system.
-   Use the calculated `gp_vmem` value to calculate the setting for the `vm.overcommit_ratio` operating system parameter:

    ```
    vm.overcommit_ratio = (RAM - 0.026 * gp_vmem) / RAM
    ```

-   Use `statement_mem` to allocate memory used for a query per segment db.
-   Use resource queues to set both the numbers of active queries \(`ACTIVE_STATEMENTS`\) and the amount of memory \(`MEMORY_LIMIT`\) that can be utilized by queries in the queue.
-   Associate all users with a resource queue. Do not use the default queue.
-   Set `PRIORITY` to match the real needs of the queue for the workload and time of day. Avoid using MAX priority.
-   Ensure that resource queue memory allocations do not exceed the setting for `gp_vmem_protect_limit`.
-   Dynamically update resource queue settings to match daily operations flow.

See [Setting the Greenplum Recommended OS Parameters](../install_guide/prep_os.html#topic3__sysctl_file) and [Memory and Resource Management with Resource Queues](workloads.html).

## <a id="part"></a>Partitioning 

-   Partition large tables only. Do not partition small tables.
-   Use partitioning only if partition elimination \(partition pruning\) can be achieved based on the query criteria.
-   Choose range partitioning over list partitioning.
-   Partition the table based on a commonly-used column, such as a date column.
-   Never partition and distribute tables on the same column.
-   Do not use default partitions.
-   Do not use multi-level partitioning; create fewer partitions with more data in each partition.
-   Validate that queries are selectively scanning partitioned tables \(partitions are being eliminated\) by examining the query `EXPLAIN` plan.
-   Do not create too many partitions with column-oriented storage because of the total number of physical files on every segment: `physical files = segments x columns x partitions`

See [Partitioning](schema.html).

## <a id="indexes"></a>Indexes 

-   In general indexes are not needed in Greenplum Database.
-   Create an index on a single column of a columnar table for drill-through purposes for high cardinality tables that require queries with high selectivity.
-   Do not index columns that are frequently updated.
-   Consider dropping indexes before loading data into a table. After the load, re-create the indexes for the table.
-   Create selective B-tree indexes.
-   Do not create bitmap indexes on columns that are updated.
-   Avoid using bitmap indexes for unique columns, very high or very low cardinality data. Bitmap indexes perform best when the column has a low cardinality—100 to 100,000 distinct values.
-   Do not use bitmap indexes for transactional workloads.
-   In general do not index partitioned tables. If indexes are needed, the index columns must be different than the partition columns.

See [Indexes](schema.html).

## <a id="_Toc286661611"></a>Resource Queues 

-   Use resource queues to manage the workload on the cluster.
-   Associate all roles with a user-defined resource queue.
-   Use the `ACTIVE_STATEMENTS` parameter to limit the number of active queries that members of the particular queue can run concurrently.
-   Use the `MEMORY_LIMIT` parameter to control the total amount of memory that queries running through the queue can utilize.
-   Alter resource queues dynamically to match the workload and time of day.

See [Configuring Resource Queues](workloads.html#configuring_rq).

## <a id="monmat"></a>Monitoring and Maintenance 

-   Implement the "Recommended Monitoring and Maintenance Tasks" in the *Greenplum Database Administrator Guide*.
-   Run `gpcheckperf` at install time and periodically thereafter, saving the output to compare system performance over time.
-   Use all the tools at your disposal to understand how your system behaves under different loads.
-   Examine any unusual event to determine the cause.
-   Monitor query activity on the system by running explain plans periodically to ensure the queries are running optimally.
-   Review plans to determine whether index are being used and partition elimination is occurring as expected.
-   Know the location and content of system log files and monitor them on a regular basis, not just when problems arise.

See [System Monitoring and Maintenance](maintenance.html), [Query Profiling](../admin_guide/query/topics/query-profiling.html#in198649) and [Monitoring Greenplum Database Log Files](logfiles.html).

## <a id="_Toc286661612"></a>ANALYZE 

-   Determine if analyzing the database is actually needed. Analyzing is not needed if `gp_autostats_mode` is set to `on_no_stats` \(the default\) and the table is not partitioned.
-   Use `analyzedb` in preference to `ANALYZE` when dealing with large sets of tables, as it does not require analyzing the entire database. The `analyzedb` utility updates statistics data for the specified tables incrementally and concurrently. For append optimized tables, `analyzedb` updates statistics incrementally only if the statistics are not current. For heap tables, statistics are always updated. `ANALYZE` does not update the table metadata that the `analyzedb` utility uses to determine whether table statistics are up to date.
-   Selectively run `ANALYZE` at the table level when needed.
-   Always run `ANALYZE` after `INSERT`, `UPDATE`. and `DELETE` operations that significantly changes the underlying data.
-   Always run `ANALYZE` after `CREATE INDEX` operations.
-   If `ANALYZE` on very large tables takes too long, run `ANALYZE` only on the columns used in a join condition, `WHERE` clause, `SORT`, `GROUP BY`, or `HAVING` clause.
-   When dealing with large sets of tables, use `analyzedb` instead of `ANALYZE.`
-   Run `analyzedb` on the root partition any time that you add a new partition(s) to a partitioned table. This operation both analyzes the child leaf partitions in parallel and merges any updated statistics into the root partition.

See [Updating Statistics with ANALYZE](analyze.html).

## <a id="_Toc286661609"></a>Vacuum 

-   Run `VACUUM` after large `UPDATE` and `DELETE` operations.
-   Do not run `VACUUM FULL`. Instead run a `CREATE TABLE...AS` operation, then rename and drop the original table.
-   Frequently run `VACUUM` on the system catalogs to avoid catalog bloat and the need to run `VACUUM FULL` on catalog tables.
-   Never issue a `kill` command against `VACUUM` on catalog tables.

See [Managing Bloat in a Database](bloat.html).

## <a id="_Toc286661610"></a>Loading 

-   Maximize the parallelism as the number of segments increase.
-   Spread the data evenly across as many ETL nodes as possible.
    -   Split very large data files into equal parts and spread the data across as many file systems as possible.
    -   Run two `gpfdist` instances per file system.
    -   Run `gpfdist` on as many interfaces as possible.
    -   Use `gp_external_max_segs` to control the number of segments that will request data from the `gpfdist` process.
    -   Always keep `gp_external_max_segs` and the number of `gpfdist` processes an even factor.
-   Always drop indexes before loading into existing tables and re-create the index after loading.
-   Run `VACUUM` after load errors to recover space.

See [Loading Data](data_loading.html).

## <a id="secty"></a>Security 

-   Secure the `gpadmin` user id and only allow essential system administrators access to it.
-   Administrators should only log in to Greenplum as `gpadmin` when performing certain system maintenance tasks \(such as upgrade or expansion\).
-   Limit users who have the `SUPERUSER` role attribute. Roles that are superusers bypass all access privilege checks in Greenplum Database, as well as resource queuing. Only system administrators should be given superuser rights. See "Altering Role Attributes" in the *Greenplum Database Administrator Guide*.
-   Database users should never log on as `gpadmin`, and ETL or production workloads should never run as `gpadmin`.
-   Assign a distinct Greenplum Database role to each user, application, or service that logs in.
-   For applications or web services, consider creating a distinct role for each application or service.
-   Use groups to manage access privileges.
-   Protect the root password.
-   Enforce a strong password password policy for operating system passwords.
-   Ensure that important operating system files are protected.

See [Security](security.html).

## <a id="enc1"></a>Encryption 

-   Encrypting and decrypting data has a performance cost; only encrypt data that requires encryption.
-   Do performance testing before implementing any encryption solution in a production system.
-   Server certificates in a production Greenplum Database system should be signed by a certificate authority \(CA\) so that clients can authenticate the server. The CA may be local if all clients are local to the organization.
-   Client connections to Greenplum Database should use SSL encryption whenever the connection goes through an insecure link.
-   A symmetric encryption scheme, where the same key is used to both encrypt and decrypt, has better performance than an asymmetric scheme and should be used when the key can be shared safely.
-   Use cryptographic functions to encrypt data on disk. The data is encrypted and decrypted in the database process, so it is important to secure the client connection with SSL to avoid transmitting unencrypted data.
-   Use the gpfdists protocol to secure ETL data as it is loaded into or unloaded from the database.

See [Encrypting Data and Database Connections](encryption.html)

## <a id="havail"></a>High Availability 

> **Note** The following guidelines apply to actual hardware deployments, but not to public cloud-based infrastructure, where high availability solutions may already exist.

-   Use a hardware RAID storage solution with 8 to 24 disks.
-   Use RAID 1, 5, or 6 so that the disk array can tolerate a failed disk.
-   Configure a hot spare in the disk array to allow rebuild to begin automatically when disk failure is detected.
-   Protect against failure of the entire disk array and degradation during rebuilds by mirroring the RAID volume.
-   Monitor disk utilization regularly and add additional space when needed.
-   Monitor segment skew to ensure that data is distributed evenly and storage is consumed evenly at all segments.
-   Set up a standby coordinator instance to take over if the primary coordinator fails.
-   Plan how to switch clients to the new coordinator instance when a failure occurs, for example, by updating the coordinator address in DNS.
-   Set up monitoring to send notifications in a system monitoring application or by email when the primary fails.
-   Set up mirrors for all segments.
-   Locate primary segments and their mirrors on different hosts to protect against host failure.
-   Recover failed segments promptly, using the `gprecoverseg` utility, to restore redundancy and return the system to optimal balance.
-   Consider a Dual Cluster configuration to provide an additional level of redundancy and additional query processing throughput.
-   Backup Greenplum databases regularly unless the data is easily restored from sources.
-   If backups are saved to local cluster storage, move the files to a safe, off-cluster location when the backup is complete.
-   If backups are saved to NFS mounts, use a scale-out NFS solution such as Dell EMC Isilon to prevent IO bottlenecks.
-   Consider using Greenplum integration to stream backups to the Dell EMC Data Domain enterprise backup platform.

See [High Availability](ha.html).

**Parent topic:** [Greenplum Database Best Practices](intro.html)

