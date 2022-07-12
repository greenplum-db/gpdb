# Planning Greenplum System Expansion 

<div class='rhs-center article-wrapper'>
<!-- Authored content placeholder -->
<p>Careful planning will help to ensure a successful Greenplum expansion project.</p> 
<p>The topics in this section help to ensure that you are prepared to perform a system expansion.</p> 
<ul> 
 <li><a href="#topic4">System Expansion Checklist</a> is a checklist you can use to prepare for and perform the system expansion process.</li> 
 <li><a href="#topic5">Planning New Hardware Platforms</a> covers planning for acquiring and setting up the new hardware.</li> 
 <li><a href="#topic6">Planning New Segment Initialization</a> provides information about planning to initialize new segment hosts with <code>gpexpand</code>.</li> 
 <li><a href="#topic10">Planning Table Redistribution</a> provides information about planning the data redistribution after the new segment hosts have been initialized.</li> 
</ul> 
<p><strong>Parent topic:</strong> <a href="GUID-admin_guide-expand-expand-main.html">Expanding a Greenplum System</a></p> 
<div class="topic nested1" xml:lang="en" lang="en" id="topic4">
    <h2 class="title topictitle2">System Expansion Checklist</h2>
    <div class="body">
      <div class="p">This checklist summarizes the tasks for a Greenplum Database system expansion. <br/>
<div class="tablenoborder">
<table cellpadding="4" cellspacing="0" summary="" id="topic4__table_pvq_yzl_2r" class="table" frame="border" border="1" rules="all">
<caption><span class="tablecap">Table 1. Greenplum Database System Expansion Checklist</span></caption>
            <tbody class="tbody">
              <tr class="row">
                <td class="entry" colspan="2" valign="top">
<p class="p"><strong class="ph b">Online Pre-Expansion Tasks</strong></p><br/>
                  <span class="ph">* System is up and available</span>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_gr2_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Devise and execute a plan for ordering, building, and networking new hardware
                  platforms, or provisioning cloud resources. </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_ryl_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Devise a database expansion plan. Map the number of segments per host,
                  schedule the downtime period for testing performance and creating the expansion
                  schema, and schedule the intervals for table redistribution.</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_e2s_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Perform a complete schema dump.</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_yq5_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Install Greenplum Database binaries on new hosts. </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_vxw_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Copy SSH keys to the new hosts (<code class="ph codeph">gpssh-exkeys</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_zrz_s1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Validate the operating system environment of the new hardware or cloud
                  resources (<code class="ph codeph">gpcheck</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_qkb_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Validate disk I/O and memory bandwidth of the new hardware or cloud resources
                  (<code class="ph codeph">gpcheckperf</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_ojd_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Validate that the master data directory has no extremely large files in the
                    <code class="ph codeph">pg_log</code> or <code class="ph codeph">gpperfmon/data</code>
                  <span class="ph">directories</span>.</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_wch_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Validate that there are no catalog issues
                  (<code class="ph codeph">gpcheckcat</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_ct3_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Prepare an expansion input file (<code class="ph codeph">gpexpand</code>). </td>
              </tr>
              <tr class="row">
                <td class="entry" colspan="2" valign="top">
<p class="p"><strong class="ph b">Offline Expansion Tasks</strong></p><br/>
                  <span class="ph">* The system is locked and unavailable to all user activity
                    during this process.</span>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_hgm_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Validate the operating system environment of the combined existing and new
                  hardware or cloud resources (<code class="ph codeph">gpcheck</code>). </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_q3q_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Validate disk I/O and memory bandwidth of the combined existing and new
                  hardware or cloud resources (<code class="ph codeph">gpcheckperf</code>). </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_rcs_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Initialize new segments into the system and create an expansion schema
                    (<code class="ph codeph">gpexpand -i <var class="keyword varname">input_file</var></code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" colspan="2" valign="top">
<p class="p"><strong class="ph b">Online Expansion and Table
                    Redistribution</strong></p><br/>
                  <span class="ph">* System is up and available</span>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_jzy_t1m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Before you start table redistribution, stop any automated snapshot processes
                  or other processes that consume disk space.</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_aq1_51m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Redistribute tables through the expanded system
                  (<code class="ph codeph">gpexpand</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_xjc_51m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">Remove expansion schema (<code class="ph codeph">gpexpand -c</code>).</td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="11.173184357541901%">
                  <img class="image" id="topic4__image_sk2_51m_2r" src="../graphics/green-checkbox.jpg" width="29" height="28">
                </td>
                <td class="entry" valign="top" width="88.82681564245812%">
<strong class="ph b">Important:</strong> Run <code class="ph codeph">analyze</code> to update distribution
                    statistics.<p class="p">During the expansion, use <code class="ph codeph">gpexpand -a</code>, and
                    post-expansion, use <code class="ph codeph">analyze</code>.</p>
</td>
              </tr>
            </tbody>
          </table>
</div>
</div>
    </div>
  </div>
<h2 id="planning-new-hardware-platforms"><a id="topic5"></a>Planning New Hardware Platforms</h2> 
<p>A deliberate, thorough approach to deploying compatible hardware greatly minimizes risk to the expansion process.</p> 
<p>Hardware resources and configurations for new segment hosts should match those of the existing hosts. Work with <em>VMware Support</em> before making a hardware purchase to expand Greenplum Database.</p> 
<p>The steps to plan and set up new hardware platforms vary for each deployment. Some considerations include how to:</p> 
<ul> 
 <li>Prepare the physical space for the new hardware; consider cooling, power supply, and other physical factors.</li> 
 <li>Determine the physical networking and cabling required to connect the new and existing hardware.</li> 
 <li>Map the existing IP address spaces and develop a networking plan for the expanded system.</li> 
 <li>Capture the system configuration (users, profiles, NICs, and so on) from existing hardware to use as a detailed list for ordering new hardware.</li> 
 <li>Create a custom build plan for deploying hardware with the desired configuration in the particular site and environment.</li> 
</ul> 
<p>After selecting and adding new hardware to your network environment, ensure you perform the tasks described in <a href="GUID-admin_guide-expand-expand-nodes.html">Preparing and Adding Hosts</a>.</p> 
<h2 id="planning-new-segment-initialization"><a id="topic6"></a>Planning New Segment Initialization</h2> 
<p>Expanding Greenplum Database requires a limited period of system downtime. During this period, run <code>gpexpand</code> to initialize new segments into the array and create an expansion schema.</p> 
<p>The time required depends on the number of schema objects in the Greenplum system and other factors related to hardware performance. In most environments, the initialization of new segments requires less than thirty minutes offline.</p> 
<p>These utilities cannot be run while <code>gpexpand</code> is performing segment initialization.</p> 
<ul> 
 <li><code>gpbackup</code></li> 
 <li><code>gpcheckcat</code></li> 
 <li><code>gpconfig</code></li> 
 <li><code>gppkg</code></li> 
 <li><code>gprestore</code></li> 
</ul> 
<p><strong>Important:</strong> After you begin initializing new segments, you can no longer restore the system using backup files created for the pre-expansion system. When initialization successfully completes, the expansion is committed and cannot be rolled back.</p> 
<h3 id="planning-mirror-segments"><a id="topic7"></a>Planning Mirror Segments</h3> 
<p>If your existing system has mirror segments, the new segments must have mirroring configured. If there are no mirrors configured for existing segments, you cannot add mirrors to new hosts with the <code>gpexpand</code> utility. For more information about segment mirroring configurations that are available during system initialization, see <a href="GUID-admin_guide-highavail-topics-g-overview-of-segment-mirroring.html#mirror_configs">Overview of Segment Mirroring Configurations</a>.</p> 
<p>For Greenplum Database systems with mirror segments, ensure you add enough new host machines to accommodate new mirror segments. The number of new hosts required depends on your mirroring strategy:</p> 
<ul> 
 <li><strong>Group Mirroring</strong> — Add at least two new hosts so the mirrors for the first host can reside on the second host, and the mirrors for the second host can reside on the first. This is the default type of mirroring if you enable segment mirroring during system initialization.</li> 
 <li><strong>Spread Mirroring</strong> — Add at least one more host to the system than the number of segments per host. The number of separate hosts must be greater than the number of segment instances per host to ensure even spreading. You can specify this type of mirroring during system initialization or when you enable segment mirroring for an existing system.</li> 
 <li><strong>Block Mirroring</strong> — Adding one or more blocks of host systems. For example add a block of four or eight hosts. Block mirroring is a custom mirroring configuration.</li> 
</ul> 
<h3 id="increasing-segments-per-host"><a id="topic8"></a>Increasing Segments Per Host</h3> 
<p>By default, new hosts are initialized with as many primary segments as existing hosts have. You can increase the segments per host or add new segments to existing hosts.</p> 
<p>For example, if existing hosts currently have two segments per host, you can use <code>gpexpand</code> to initialize two additional segments on existing hosts for a total of four segments and initialize four new segments on new hosts.</p> 
<p>The interactive process for creating an expansion input file prompts for this option; you can also specify new segment directories manually in the input configuration file. For more information, see <a href="GUID-admin_guide-expand-expand-initialize.html">Creating an Input File for System Expansion</a>.</p> 
<h3 id="about-the-expansion-schema"><a id="topic9"></a>About the Expansion Schema</h3> 
<p>At initialization, <code>gpexpand</code> creates an expansion schema. If you do not specify a database at initialization (<code>gpexpand -D</code>), the schema is created in the database indicated by the PGDATABASE environment variable.</p> 
<p>The expansion schema stores metadata for each table in the system so its status can be tracked throughout the expansion process. The expansion schema consists of two tables and a view for tracking expansion operation progress:</p> 
<ul> 
 <li><em>gpexpand.status</em></li> 
 <li><em>gpexpand.status_detail</em></li> 
 <li><em>gpexpand.expansion_progress</em></li> 
</ul> 
<p>Control expansion process aspects by modifying <em>gpexpand.status_detail</em>. For example, removing a record from this table prevents the system from expanding the table across new segments. Control the order in which tables are processed for redistribution by updating the <code>rank</code> value for a record. For more information, see <a href="GUID-admin_guide-expand-expand-redistribute.html">Ranking Tables for Redistribution</a>.</p> 
<h2 id="planning-table-redistribution"><a id="topic10"></a>Planning Table Redistribution</h2> 
<p>Table redistribution is performed while the system is online. For many Greenplum systems, table redistribution completes in a single <code>gpexpand</code> session scheduled during a low-use period. Larger systems may require multiple sessions and setting the order of table redistribution to minimize performance impact. Complete the table redistribution in one session if possible.</p> 
<p><strong>Important:</strong> To perform table redistribution, your segment hosts must have enough disk space to temporarily hold a copy of your largest table. All tables are unavailable for read and write operations during redistribution.</p> 
<p>The performance impact of table redistribution depends on the size, storage type, and partitioning design of a table. For any given table, redistributing it with <code>gpexpand</code> takes as much time as a <code>CREATE TABLE AS SELECT</code> operation would. When redistributing a terabyte-scale fact table, the expansion utility can use much of the available system resources, which could affect query performance or other database workloads.</p> 
<h3 id="managing-redistribution-in-large-scale-greenplum-systems"><a id="topic11"></a>Managing Redistribution in Large-Scale Greenplum Systems</h3> 
<p>You can manage the order in which tables are redistributed by adjusting their ranking. See <a href="GUID-admin_guide-expand-expand-redistribute.html">Ranking Tables for Redistribution</a>. Manipulating the redistribution order can help adjust for limited disk space and restore optimal query performance for high-priority queries sooner.</p>
<p>When planning the redistribution phase, consider the impact of the exclusive lock taken on each table during redistribution. User activity on a table can delay its redistribution, but also tables are unavailable for user activity during redistribution.</p>  
<h4 id="systems-with-abundant-free-disk-space"><a id="systs"></a>Systems with Abundant Free Disk Space</h4> 
<p>In systems with abundant free disk space (required to store a copy of the largest table), you can focus on restoring optimum query performance as soon as possible by first redistributing important tables that queries use heavily. Assign high ranking to these tables, and schedule redistribution operations for times of low system usage. Run one redistribution process at a time until large or critical tables have been redistributed.</p> 
<h4 id="systems-with-limited-free-disk-space"><a id="systslim"></a>Systems with Limited Free Disk Space</h4> 
<p>If your existing hosts have limited disk space, you may prefer to first redistribute smaller tables (such as dimension tables) to clear space to store a copy of the largest table. Available disk space on the original segments increases as each table is redistributed across the expanded system. When enough free space exists on all segments to store a copy of the largest table, you can redistribute large or critical tables. Redistribution of large tables requires exclusive locks; schedule this procedure for off-peak hours.</p> 
<p>Also consider the following:</p> 
<ul> 
 <li>Run multiple parallel redistribution processes during off-peak hours to maximize available system resources.</li> 
 <li>When running multiple processes, operate within the connection limits for your Greenplum system. For information about limiting concurrent connections, see <a href="GUID-admin_guide-client_auth.html">Limiting Concurrent Connections</a>.</li> 
</ul> 
<h3 id="redistributing-append-optimized-and-compressed-tables"><a id="topic13"></a>Redistributing Append-Optimized and Compressed Tables</h3> 
<p><code>gpexpand</code> redistributes append-optimized and compressed append-optimized tables at different rates than heap tables. The CPU capacity required to compress and decompress data tends to increase the impact on system performance. For similar-sized tables with similar data, you may find overall performance differences like the following:</p> 
<ul> 
 <li>Uncompressed append-optimized tables expand 10% faster than heap tables.</li> 
 <li><code>zlib</code>-compressed append-optimized tables expand at a significantly slower rate than uncompressed append-optimized tables, potentially up to 80% slower.</li> 
 <li>Systems with data compression such as ZFS/LZJB take longer to redistribute.</li> 
</ul> 
<p><strong>Important:</strong> If your system hosts use data compression, use identical compression settings on the new hosts to avoid disk space shortage.</p>
    <div class="topic nested2" xml:lang="en" lang="en" id="topic14">
      <h3 class="title topictitle3">Redistributing Tables with Primary Key Constraints</h3>
      <div class="body">
        <p class="p">There is a time period during which primary key constraints cannot be enforced between
          the initialization of new segments and successful table redistribution. Duplicate data
          inserted into tables during this period prevents the expansion utility from redistributing
          the affected tables.</p>
        <div class="p">After a table is redistributed, the primary key constraint is properly enforced again. If
          an expansion process violates constraints, the expansion utility logs errors and displays
          warnings when it completes. To fix constraint violations, perform one of the following
            remedies:<ul class="ul" id="topic14__ul_g1d_jfr_g4">
            <li class="li" id="topic14__no169634">Clean up duplicate data in the primary key columns, and re-run
                <code class="ph codeph">gpexpand</code>. </li>
            <li class="li" id="topic14__no169656">Drop the primary key constraints, and re-run
              <code class="ph codeph">gpexpand</code>. </li>
          </ul>
</div>
      </div>
    </div>
    <div class="topic nested2" xml:lang="en" lang="en" id="topic15">
      <h3 class="title topictitle3">Redistributing Tables with User-Defined Data Types</h3>
      <div class="body">
        <p class="p">You cannot perform redistribution with the expansion utility on tables with dropped
          columns of user-defined data types. To redistribute tables with dropped columns of
          user-defined types, first re-create the table using <code class="ph codeph">CREATE TABLE AS
            SELECT</code>. After this process removes the dropped columns, redistribute the table
          with <code class="ph codeph">gpexpand</code>.</p>
      </div>
    </div>
<h3 id="redistributing-partitioned-tables"><a id="topic16"></a>Redistributing Partitioned Tables</h3> 
<p>Because the expansion utility can process each individual partition on a large table, an efficient partition design reduces the performance impact of table redistribution. Only the child tables of a partitioned table are set to a random distribution policy. The read/write lock for redistribution applies to only one child table at a time.</p> 
<h3 id="redistributing-indexed-tables"><a id="topic_qq2_x3r_g4"></a>Redistributing Indexed Tables</h3> 
<p>Because the <code>gpexpand</code> utility must re-index each indexed table after redistribution, a high level of indexing has a large performance impact. Systems with intensive indexing have significantly slower rates of table redistribution.</p>

</div>