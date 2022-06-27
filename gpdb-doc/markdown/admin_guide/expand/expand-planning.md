# Planning Greenplum System Expansion 

Careful planning will help to ensure a successful Greenplum expansion project.

The topics in this section help to ensure that you are prepared to execute a system expansion.

-   [System Expansion Checklist](#topic4) is a checklist you can use to prepare for and execute the system expansion process.
-   [Planning New Hardware Platforms](#topic5) covers planning for acquiring and setting up the new hardware.
-   [Planning New Segment Initialization](#topic6) provides information about planning to initialize new segment hosts with `gpexpand`.
-   [Planning Table Redistribution](#topic10) provides information about planning the data redistribution after the new segment hosts have been initialized.

**Parent topic:** [Expanding a Greenplum System](../expand/expand-main.html)

## System Expansion Checklist 

This checklist summarizes the tasks for a Greenplum Database system expansion.

|**Online Pre-Expansion Tasks**

 \* System is up and available|
|![](../graphics/green-checkbox.jpg)|Devise and execute a plan for ordering, building, and networking new hardware platforms, or provisioning cloud resources.|
|![](../graphics/green-checkbox.jpg)|Devise a database expansion plan. Map the number of segments per host, schedule the downtime period for testing performance and creating the expansion schema, and schedule the intervals for table redistribution.|
|![](../graphics/green-checkbox.jpg)|Perform a complete schema dump.|
|![](../graphics/green-checkbox.jpg)|Install Greenplum Database binaries on new hosts.|
|![](../graphics/green-checkbox.jpg)|Copy SSH keys to the new hosts \(`gpssh-exkeys`\).|
|![](../graphics/green-checkbox.jpg)|Validate the operating system environment of the new hardware or cloud resources \(`gpcheck`\).|
|![](../graphics/green-checkbox.jpg)|Validate disk I/O and memory bandwidth of the new hardware or cloud resources \(`gpcheckperf`\).|
|![](../graphics/green-checkbox.jpg)|Validate that the master data directory has no extremely large files in the `pg_log` or `gpperfmon/data` directories.|
|![](../graphics/green-checkbox.jpg)|Validate that there are no catalog issues \(`gpcheckcat`\).|
|![](../graphics/green-checkbox.jpg)|Prepare an expansion input file \(`gpexpand`\).|
|**Offline Expansion Tasks**

 \* The system is locked and unavailable to all user activity during this process.|
|![](../graphics/green-checkbox.jpg)|Validate the operating system environment of the combined existing and new hardware or cloud resources \(`gpcheck`\).|
|![](../graphics/green-checkbox.jpg)|Validate disk I/O and memory bandwidth of the combined existing and new hardware or cloud resources \(`gpcheckperf`\).|
|![](../graphics/green-checkbox.jpg)|Initialize new segments into the system and create an expansion schema \(`gpexpand -i input\_file`\).|
|**Online Expansion and Table Redistribution**

 \* System is up and available|
|![](../graphics/green-checkbox.jpg)|Before you start table redistribution, stop any automated snapshot processes or other processes that consume disk space.|
|![](../graphics/green-checkbox.jpg)|Redistribute tables through the expanded system \(`gpexpand`\).|
|![](../graphics/green-checkbox.jpg)|Remove expansion schema \(`gpexpand -c`\).|
|![](../graphics/green-checkbox.jpg)|**Important:** Run `analyze` to update distribution statistics.During the expansion, use `gpexpand -a`, and post-expansion, use `analyze`.

|

## Planning New Hardware Platforms 

A deliberate, thorough approach to deploying compatible hardware greatly minimizes risk to the expansion process.

Hardware resources and configurations for new segment hosts should match those of the existing hosts. Work with *Greenplum Platform Engineering* before making a hardware purchase to expand Greenplum Database.

The steps to plan and set up new hardware platforms vary for each deployment. Some considerations include how to:

-   Prepare the physical space for the new hardware; consider cooling, power supply, and other physical factors.
-   Determine the physical networking and cabling required to connect the new and existing hardware.
-   Map the existing IP address spaces and developing a networking plan for the expanded system.
-   Capture the system configuration \(users, profiles, NICs, and so on\) from existing hardware to use as a detailed list for ordering new hardware.
-   Create a custom build plan for deploying hardware with the desired configuration in the particular site and environment.

After selecting and adding new hardware to your network environment, ensure you perform the tasks described in [Preparing and Adding Nodes](expand-nodes.html).

## Planning New Segment Initialization 

Expanding Greenplum Database requires a limited period of system downtime. During this period, run `gpexpand` to initialize new segments into the array and create an expansion schema.

The time required depends on the number of schema objects in the Greenplum system and other factors related to hardware performance. In most environments, the initialization of new segments requires less than thirty minutes offline.

These utilities cannot be run while `gpexpand` is performing segment initialization.

-   `gpbackup`
-   `gpcheckcat`
-   `gpconfig`
-   `gppkg`
-   `gprestore`

**Important:** After you begin initializing new segments, you can no longer restore the system using backup files created for the pre-expansion system. When initialization successfully completes, the expansion is committed and cannot be rolled back.

### Planning Mirror Segments 

If your existing array has mirror segments, the new segments must have mirroring configured. If there are no mirrors configured for existing segments, you cannot add mirrors to new hosts with the `gpexpand` utility. For more information about segment mirroring configurations that are available during system initialization, see [../highavail/topics/g-overview-of-segment-mirroring.md\#mirror\_configs](../highavail/topics/g-overview-of-segment-mirroring.md#mirror_configs).

For Greenplum Database systems with mirror segments, ensure you add enough new host machines to accommodate new mirror segments. The number of new hosts required depends on your mirroring strategy:

-   **Group Mirroring** — Add at least two new hosts so the mirrors for the first host can reside on the second host, and the mirrors for the second host can reside on the first. This is the default type of mirroring if you enable segment mirroring during system initialization.
-   **Spread Mirroring** — Add at least one more host to the system than the number of segments per host. The number of separate hosts must be greater than the number of segment instances per host to ensure even spreading. You can specify this type of mirroring during system initialization or when you enable segment mirroring for an existing system.
-   **Block Mirroring** — Adding one or more blocks of host systems. For example add a block of four or eight hosts. Block mirroring is a custom mirroring configuration.

### Increasing Segments Per Host 

By default, new hosts are initialized with as many primary segments as existing hosts have. You can increase the segments per host or add new segments to existing hosts.

For example, if existing hosts currently have two segments per host, you can use `gpexpand` to initialize two additional segments on existing hosts for a total of four segments and four new segments on new hosts.

The interactive process for creating an expansion input file prompts for this option; you can also specify new segment directories manually in the input configuration file. For more information, see [Creating an Input File for System Expansion](expand-initialize.html).

### About the Expansion Schema 

At initialization, `gpexpand` creates an expansion schema. If you do not specify a database at initialization \(`gpexpand -D`\), the schema is created in the database indicated by the `PGDATABASE` environment variable.

The expansion schema stores metadata for each table in the system so its status can be tracked throughout the expansion process. The expansion schema consists of two tables and a view for tracking expansion operation progress:

-   *gpexpand.status*
-   *gpexpand.status\_detail*
-   *gpexpand.expansion\_progress*

Control expansion process aspects by modifying *gpexpand.status\_detail*. For example, removing a record from this table prevents the system from expanding the table across new segments. Control the order in which tables are processed for redistribution by updating the `rank` value for a record. For more information, see [Ranking Tables for Redistribution](expand-redistribute.html).

## Planning Table Redistribution 

Table redistribution is performed while the system is online. For many Greenplum systems, table redistribution completes in a single `gpexpand` session scheduled during a low-use period. Larger systems may require multiple sessions and setting the order of table redistribution to minimize performance impact. Complete the table redistribution in one session if possible.

**Important:** To perform table redistribution, your segment hosts must have enough disk space to temporarily hold a copy of your largest table. All tables are unavailable for read and write operations during redistribution.

The performance impact of table redistribution depends on the size, storage type, and partitioning design of a table. For any given table, redistributing it with `gpexpand` takes as much time as a `CREATE TABLE AS SELECT` operation would. When redistributing a terabyte-scale fact table, the expansion utility can use much of the available system resources, which could affect query performance or other database workloads.

### Managing Redistribution in Large-Scale Greenplum Systems 

You can manage the order in which tables are redistributed by adjusting their ranking. See [Ranking Tables for Redistribution](expand-redistribute.html). Manipulating the redistribution order can help adjust for limited disk space and restore optimal query performance for high-priority queries sooner.

When planning the redistribution phase, consider the impact of the exclusive lock taken on each table during redistribution. User activity on a table can delay its redistribution, but also tables are unavailable for user activity during redistribution.

#### Systems with Abundant Free Disk Space 

In systems with abundant free disk space \(required to store a copy of the largest table\), you can focus on restoring optimum query performance as soon as possible by first redistributing important tables that queries use heavily. Assign high ranking to these tables, and schedule redistribution operations for times of low system usage. Run one redistribution process at a time until large or critical tables have been redistributed.

#### Systems with Limited Free Disk Space 

If your existing hosts have limited disk space, you may prefer to first redistribute smaller tables \(such as dimension tables\) to clear space to store a copy of the largest table. Available disk space on the original segments increases as each table is redistributed across the expanded system. When enough free space exists on all segments to store a copy of the largest table, you can redistribute large or critical tables. Redistribution of large tables requires exclusive locks; schedule this procedure for off-peak hours.

Also consider the following:

-   Run multiple parallel redistribution processes during off-peak hours to maximize available system resources.
-   When running multiple processes, operate within the connection limits for your Greenplum system. For information about limiting concurrent connections, see [Limiting Concurrent Connections](../client_auth.html).

### Redistributing Append-Optimized and Compressed Tables 

`gpexpand` redistributes append-optimized and compressed append-optimized tables at different rates than heap tables. The CPU capacity required to compress and decompress data tends to increase the impact on system performance. For similar-sized tables with similar data, you may find overall performance differences like the following:

-   Uncompressed append-optimized tables expand 10% faster than heap tables.
-   `zlib`-compressed append-optimized tables expand at a significantly slower rate than uncompressed append-optimized tables, potentially up to 80% slower.
-   Systems with data compression such as ZFS/LZJB take longer to redistribute.

**Important:** If your system nodes use data compression, use identical compression on new nodes to avoid disk space shortage.

### Redistributing Tables with Primary Key Constraints 

There is a time period during which primary key constraints cannot be enforced between the initialization of new segments and successful table redistribution. Duplicate data inserted into tables during this period prevents the expansion utility from redistributing the affected tables.

After a table is redistributed, the primary key constraint is properly enforced again. If an expansion process violates constraints, the expansion utility logs errors and displays warnings when it completes. To fix constraint violations, perform one of the following remedies:

-   Clean up duplicate data in the primary key columns, and re-run `gpexpand`.
-   Drop the primary key constraints, and re-run `gpexpand`.

### Redistributing Tables with User-Defined Data Types 

You cannot perform redistribution with the expansion utility on tables with dropped columns of user-defined data types. To redistribute tables with dropped columns of user-defined types, first re-create the table using `CREATE TABLE AS SELECT`. After this process removes the dropped columns, redistribute the table with `gpexpand`.

### Redistributing Partitioned Tables 

Because the expansion utility can process each individual partition on a large table, an efficient partition design reduces the performance impact of table redistribution. Only the child tables of a partitioned table are set to a random distribution policy. The read/write lock for redistribution applies to only one child table at a time.

### Redistributing Indexed Tables 

Because the `gpexpand` utility must re-index each indexed table after redistribution, a high level of indexing has a large performance impact. Systems with intensive indexing have significantly slower rates of table redistribution.

