---
title: Planning Greenplum System Expansion 
---

Careful planning will help to ensure a successful Greenplum expansion project.

The topics in this section help to ensure that you are prepared to perform a system expansion.

-   [System Expansion Checklist](#topic4) is a checklist you can use to prepare for and perform the system expansion process.
-   [Planning New Hardware Platforms](#topic5) covers planning for acquiring and setting up the new hardware.
-   [Planning New Segment Initialization](#topic6) provides information about planning to initialize new segment hosts with `gpexpand`.
-   [Planning Table Redistribution](#topic10) provides information about planning the data redistribution after the new segment hosts have been initialized.

**Important:** When expanding a Greenplum Database system, you must disable Greenplum interconnect proxies before adding new hosts and segment instances to the system, and you must update the `gp_interconnect_proxy_addresses` parameter with the newly-added segment instances before you re-enable interconnect proxies. For example, these commands disable Greenplum interconnect proxies by setting the interconnect to the default \(`UDPIFC`\) and reloading the `postgresql.conf` file to update the Greenplum system configuration.

```
gpconfig -r gp_interconnect_type
gpstop -u
```

For information about Greenplum interconnect proxies, see [Configuring Proxies for the Greenplum Interconnect](../managing/proxy-ic.html).

**Parent topic:**[Expanding a Greenplum System](../expand/expand-main.html)

## <a id="topic4"></a>System Expansion Checklist 

This checklist summarizes the tasks for a Greenplum Database system expansion.

<table class="table frame-all" id="topic4__table_pvq_yzl_2r"><caption><span class="table--title-label">Table 1. </span><span class="title">Greenplum Database System Expansion Checklist</span></caption><colgroup><col style="width:11.173184357541901%"><col style="width:88.82681564245812%"></colgroup><tbody class="tbody">
              <tr class="row">
                <td class="entry" colspan="2"><p class="p"><strong class="ph b">Online Pre-Expansion Tasks</strong></p>
                  <span class="ph">* System is up and available</span>
                </td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_gr2_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Plan for ordering, building, and networking new hardware
                  platforms, or provisioning cloud resources. </td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_ryl_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Devise a database expansion plan. Map the number of segments per host,
                  schedule the downtime period for testing performance and creating the expansion
                  schema, and schedule the intervals for table redistribution.</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_e2s_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Perform a complete schema dump.</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_yq5_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Install Greenplum Database binaries on new hosts. </td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_vxw_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Copy SSH keys to the new hosts (<code class="ph codeph">gpssh-exkeys</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_qkb_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Validate disk I/O and memory bandwidth of the new hardware or cloud resources
                    (<code class="ph codeph">gpcheckperf</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_ojd_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Validate that the master data directory has no extremely large files in the
                    <code class="ph codeph">log</code> directory.</td>
              </tr>
              <tr class="row">
                <td class="entry" colspan="2"><p class="p"><strong class="ph b">Offline Pre-Expansion Tasks</strong></p>
                  <span class="ph">* The system is unavailable to all user activity during this process.</span>
                </td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_wch_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Validate that there are no catalog issues
                  (<code class="ph codeph">gpcheckcat</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_q3q_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Validate disk I/O and memory bandwidth of the combined existing and new
                  hardware or cloud resources (<code class="ph codeph">gpcheckperf</code>). </td>
              </tr>
              <tr class="row">
                <td class="entry" colspan="2"><p class="p"><strong class="ph b">Online Segment Instance
                    Initialization</strong></p><span class="ph">* System is up and available</span>
                </td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_ct3_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Prepare an expansion input file (<code class="ph codeph">gpexpand</code>). </td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_rcs_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Initialize new segments into the system and create an expansion schema
                    (<code class="ph codeph">gpexpand -i <var class="keyword varname">input_file</var></code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" colspan="2"><p class="p"><strong class="ph b">Online Expansion and Table
                    Redistribution</strong></p>
                  <span class="ph">* System is up and available</span>
                </td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_jzy_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Before you start table redistribution, stop any automated snapshot processes
                  or other processes that consume disk space.</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_aq1_51m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Redistribute tables through the expanded system
                  (<code class="ph codeph">gpexpand</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_xjc_51m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry">Remove expansion schema (<code class="ph codeph">gpexpand -c</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry">
                  <img class="image" id="topic4__image_sk2_51m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry"><strong class="ph b">Important:</strong> Run <code class="ph codeph">analyze</code> to update distribution
                    statistics.<p class="p">During the expansion, use <code class="ph codeph">gpexpand -a</code>, and
                    post-expansion, use <code class="ph codeph">analyze</code>.</p></td>
              </tr>
              <tr class="row">
                <td class="entry" colspan="2"><p class="p"><strong class="ph b">Back Up Databases</strong></p><span class="ph">* System is up
                    and available</span></td>
              </tr>
              <tr class="row">
                <td class="entry"><img class="image" id="topic4__image_ogk_mj4_lhb" src="../graphics/green-checkbox.jpg" width="29" height="28"></td>
                <td class="entry">Back up databases using the <code class="ph codeph">gpbackup</code> utility. Backups you
                  created before you began the system expansion cannot be restored to the newly
                  expanded system because the <code class="ph codeph">gprestore</code> utility can only restore
                  backups to a Greenplum Database system with the same number of segments.</td>
              </tr>
            </tbody></table>

## <a id="topic5"></a>Planning New Hardware Platforms 

A deliberate, thorough approach to deploying compatible hardware greatly minimizes risk to the expansion process.

Hardware resources and configurations for new segment hosts should match those of the existing hosts. Work with *VMware Support* before making a hardware purchase to expand Greenplum Database.

The steps to plan and set up new hardware platforms vary for each deployment. Some considerations include how to:

-   Prepare the physical space for the new hardware; consider cooling, power supply, and other physical factors.
-   Determine the physical networking and cabling required to connect the new and existing hardware.
-   Map the existing IP address spaces and developing a networking plan for the expanded system.
-   Capture the system configuration \(users, profiles, NICs, and so on\) from existing hardware to use as a detailed list for ordering new hardware.
-   Create a custom build plan for deploying hardware with the desired configuration in the particular site and environment.

After selecting and adding new hardware to your network environment, ensure you perform the tasks described in [Preparing and Adding Hosts](expand-nodes.html).

## <a id="topic6"></a>Planning New Segment Initialization 

Expanding Greenplum Database can be performed when the system is up and available. Run `gpexpand` to initialize new segment instances into the system and create an expansion schema.

The time required depends on the number of schema objects in the Greenplum system and other factors related to hardware performance. In most environments, the initialization of new segments requires less than thirty minutes offline.

These utilities cannot be run while `gpexpand` is performing segment initialization.

-   `gpbackup`
-   `gpcheckcat`
-   `gpconfig`
-   `gppkg`
-   `gprestore`

**Important:** After you begin initializing new segments, you can no longer restore the system using backup files created for the pre-expansion system. When initialization successfully completes, the expansion is committed and cannot be rolled back.

### <a id="topic7"></a>Planning Mirror Segments 

If your existing system has mirror segments, the new segments must have mirroring configured. If there are no mirrors configured for existing segments, you cannot add mirrors to new hosts with the `gpexpand` utility. For more information about segment mirroring configurations that are available during system initialization, see [About Segment Mirroring Configurations](../highavail/topics/g-overview-of-segment-mirroring.html#mirror_configs).

For Greenplum Database systems with mirror segments, ensure you add enough new host machines to accommodate new mirror segments. The number of new hosts required depends on your mirroring strategy:

-   **Group Mirroring** — Add at least two new hosts so the mirrors for the first host can reside on the second host, and the mirrors for the second host can reside on the first. This is the default type of mirroring if you enable segment mirroring during system initialization.
-   **Spread Mirroring** — Add at least one more host to the system than the number of segments per host. The number of separate hosts must be greater than the number of segment instances per host to ensure even spreading. You can specify this type of mirroring during system initialization or when you enable segment mirroring for an existing system.
-   **Block Mirroring** — Adding one or more blocks of host systems. For example add a block of four or eight hosts. Block mirroring is a custom mirroring configuration. For more information about block mirroring, see [Segment Mirroring Configurations](../../best_practices/ha.html#topic_ngz_qf4_tt).

### <a id="topic8"></a>Increasing Segments Per Host 

By default, new hosts are initialized with as many primary segments as existing hosts have. You can increase the segments per host or add new segments to existing hosts.

For example, if existing hosts currently have two segments per host, you can use `gpexpand` to initialize two additional segments on existing hosts for a total of four segments and initialize four new segments on new hosts.

The interactive process for creating an expansion input file prompts for this option; you can also specify new segment directories manually in the input configuration file. For more information, see [Creating an Input File for System Expansion](expand-initialize.html).

### <a id="topic9"></a>About the Expansion Schema 

At initialization, the `gpexpand` utility creates an expansion schema named *gpexpand* in the postgres database.

The expansion schema stores metadata for each table in the system so its status can be tracked throughout the expansion process. The expansion schema consists of two tables and a view for tracking expansion operation progress:

-   *gpexpand.status*
-   *gpexpand.status\_detail*
-   *gpexpand.expansion\_progress*

Control expansion process aspects by modifying *gpexpand.status\_detail*. For example, removing a record from this table prevents the system from expanding the table across new segments. Control the order in which tables are processed for redistribution by updating the `rank` value for a record. For more information, see [Ranking Tables for Redistribution](expand-redistribute.html).

## <a id="topic10"></a>Planning Table Redistribution 

Table redistribution is performed while the system is online. For many Greenplum systems, table redistribution completes in a single `gpexpand` session scheduled during a low-use period. Larger systems may require multiple sessions and setting the order of table redistribution to minimize performance impact. Complete the table redistribution in one session if possible.

**Important:** To perform table redistribution, your segment hosts must have enough disk space to temporarily hold a copy of your largest table. All tables are unavailable for read and write operations during redistribution.

The performance impact of table redistribution depends on the size, storage type, and partitioning design of a table. For any given table, redistributing it with `gpexpand` takes as much time as a `CREATE TABLE AS SELECT` operation would. When redistributing a terabyte-scale fact table, the expansion utility can use much of the available system resources, which could affect query performance or other database workloads.

### <a id="topic11"></a>Managing Redistribution in Large-Scale Greenplum Systems 

When planning the redistribution phase, consider the impact of the `ACCESS EXCLUSIVE` lock taken on each table, and the table data redistribution method. User activity on a table can delay its redistribution, but also tables are unavailable for user activity during redistribution.

You can manage the order in which tables are redistributed by adjusting their ranking. See [Ranking Tables for Redistribution](expand-redistribute.html). Manipulating the redistribution order can help adjust for limited disk space and restore optimal query performance for high-priority queries sooner.

#### <a id="tabred"></a>Table Redistribution Methods 

There are two methods of redistributing data when performing a Greenplum Database expansion.

-   `rebuild` - Create a new table, copy all the data from the old to the new table, and replace the old table. This is the default. The rebuild method is similar to creating a new table with a `CREATE TABLE AS SELECT` command. During data redistribution, an `ACCESS EXCLUSIVE` lock is acquired on the table.
-   `move` - Scan all the data and perform an `UPDATE` operation to move rows as needed to different segment instances. During data redistribution, an `ACCESS EXCLUSIVE` lock is acquired on the table. In general, this method requires less disk space, however, it creates obsolete table rows and might require a `VACUUM` operation on the table after the data redistribution. Also, this method updates indexes one row at a time, which can be much slower than rebuilding the index with the `CREATE INDEX` command.

#### <a id="systs"></a>Systems with Abundant Free Disk Space 

In systems with abundant free disk space \(required to store a copy of the largest table\), you can focus on restoring optimum query performance as soon as possible by first redistributing important tables that queries use heavily. Assign high ranking to these tables, and schedule redistribution operations for times of low system usage. Run one redistribution process at a time until large or critical tables have been redistributed.

#### <a id="systslim"></a>Systems with Limited Free Disk Space 

If your existing hosts have limited disk space, you may prefer to first redistribute smaller tables \(such as dimension tables\) to clear space to store a copy of the largest table. Available disk space on the original segments increases as each table is redistributed across the expanded system. When enough free space exists on all segments to store a copy of the largest table, you can redistribute large or critical tables. Redistribution of large tables requires exclusive locks; schedule this procedure for off-peak hours.

Also consider the following:

-   Run multiple parallel redistribution processes during off-peak hours to maximize available system resources.
-   When running multiple processes, operate within the connection limits for your Greenplum system. For information about limiting concurrent connections, see [Limiting Concurrent Connections](../client_auth.html).

### <a id="topic13"></a>Redistributing Append-Optimized and Compressed Tables 

`gpexpand` redistributes append-optimized and compressed append-optimized tables at different rates than heap tables. The CPU capacity required to compress and decompress data tends to increase the impact on system performance. For similar-sized tables with similar data, you may find overall performance differences like the following:

-   Uncompressed append-optimized tables expand 10% faster than heap tables.
-   Append-optimized tables that are defined to use data compression expand at a significantly slower rate than uncompressed append-optimized tables, potentially up to 80% slower.
-   Systems with data compression such as ZFS/LZJB take longer to redistribute.

**Important:** If your system hosts use data compression, use identical compression settings on the new hosts to avoid disk space shortage.

### <a id="topic16"></a>Redistributing Partitioned Tables 

Because the expansion utility can process each individual partition on a large table, an efficient partition design reduces the performance impact of table redistribution. Only the child tables of a partitioned table are set to a random distribution policy. The read/write lock for redistribution applies to only one child table at a time.

### <a id="topic_qq2_x3r_g4"></a>Redistributing Indexed Tables 

Because the `gpexpand` utility must re-index each indexed table after redistribution, a high level of indexing has a large performance impact. Systems with intensive indexing have significantly slower rates of table redistribution.

