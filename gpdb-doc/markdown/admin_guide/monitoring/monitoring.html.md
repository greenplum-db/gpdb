---
title: Recommended Monitoring and Maintenance Tasks 
---

This section lists monitoring and maintenance activities recommended to ensure high availability and consistent performance of your Greenplum Database cluster.

The tables in the following sections suggest activities that a Greenplum System Administrator can perform periodically to ensure that all components of the system are operating optimally. Monitoring activities help you to detect and diagnose problems early. Maintenance activities help you to keep the system up-to-date and avoid deteriorating performance, for example, from bloated system tables or diminishing free disk space.

It is not necessary to implement all of these suggestions in every cluster; use the frequency and severity recommendations as a guide to implement measures according to your service requirements.

**Parent topic:**[Managing a Greenplum System](../managing/partII.html)

## <a id="drr_5bg_rp"></a>Database State Monitoring Activities 

<table class="table frame-all" id="drr_5bg_rp__table_drr_5bg_rp"><caption><span class="table--title-label">Table 1. </span><span class="title">Database State Monitoring Activities</span></caption><colgroup><col><col><col></colgroup><thead class="thead">
                            <tr class="row">
                                <th class="entry" id="drr_5bg_rp__table_drr_5bg_rp__entry__1">Activity</th>
                                <th class="entry" id="drr_5bg_rp__table_drr_5bg_rp__entry__2">Procedure</th>
                                <th class="entry" id="drr_5bg_rp__table_drr_5bg_rp__entry__3">Corrective Actions</th>
                            </tr>
                        </thead><tbody class="tbody">
                            <tr class="row">
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__1">List segments that are currently down. If any rows are
                                    returned, this should generate a warning or alert.<p class="p">Recommended
                                        frequency: run every 5 to 10 minutes</p><p class="p">Severity:
                                        IMPORTANT</p></td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__2">Run the following query in the <code class="ph codeph">postgres</code>
                                    database:<pre class="pre codeblock"><code>SELECT * FROM gp_segment_configuration
WHERE status = 'd';</code></pre></td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__3">If the query returns any rows, follow these steps to correct
                                    the problem:<ol class="ol" id="drr_5bg_rp__ol_okh_2cg_rp">
                                        <li class="li">Verify that the hosts with down segments are responsive. </li>
                                        <li class="li">If hosts are OK, check the <span class="ph filepath">log</span>
                                            files for the <span class="ph">primaries and
                                                mirrors of the </span>down segments to discover the
                                            root cause of the segments going down. </li>
                                        <li class="li">If no unexpected errors are found, run the
                                                <code class="ph codeph">gprecoverseg</code> utility to bring the
                                            segments back online.</li>
                                    </ol></td>
                            </tr>
                          
                            <tr class="row">
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__1">Check for segments that are up and not in sync. If rows are
                                    returned, this should generate a warning or alert.<p class="p">Recommended
                                        frequency: run every 5 to 10 minutes</p></td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__2">
                                    <div class="p">Execute the following query in the <code class="ph codeph">postgres</code>
                                        database:<pre class="pre codeblock"><code>SELECT * FROM gp_segment_configuration
WHERE mode = 'n' and status = 'u' and content &lt;&gt; -1;</code></pre></div>
                                </td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__3">If the query returns rows then the segment might be in the process
                                    of moving from  <code class="ph codeph">Not In Sync</code> to
                                        <code class="ph codeph">Synchronized</code> mode. Use <code class="ph codeph">gpstate
                                        -e</code> to track progress.</td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__1">Check for segments that are not operating in their preferred role but are marked as up and <code class="ph codeph">Synchronized</code>.
                                    If any segments are found, the cluster may not be
                                    balanced. If any rows are returned this should generate a
                                    warning or alert. <p class="p">Recommended frequency: run every 5 to 10
                                        minutes</p><p class="p">Severity: IMPORTANT</p></td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__2">
                                    <div class="p">Execute the following query in the <code class="ph codeph">postgres</code>
                                        database:
                                        <pre class="pre codeblock"><code>SELECT * FROM gp_segment_configuration 
WHERE preferred_role &lt;&gt; role  and status = 'u' and mode = 's';</code></pre></div>
                                    
                                </td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__3">
                                    <p class="p">When the segments are not running in their preferred role, processing might be skewed. 
				Run <code class="ph codeph">gprecoverseg -r</code> to bring the segments back into their preferred roles.</p>
                                </td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__1">Run a distributed query to test that it runs on all segments.
                                    One row should be returned for each primary segment.
                                        <p class="p">Recommended frequency: run every 5 to 10
                                        minutes</p><p class="p">Severity: CRITICAL</p></td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__2">
                                    <div class="p">Execute the following query in the <code class="ph codeph">postgres</code>
                                        database:<pre class="pre codeblock"><code>SELECT gp_segment_id, count(*)
FROM gp_dist_random('pg_class')
GROUP BY 1;</code></pre></div>
                                </td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__3">
                                    <p class="p">If this query fails, there is an issue dispatching to some
                                        segments in the cluster. This is a rare event. Check the
                                        hosts that are not able to be dispatched to ensure there is
                                        no hardware or networking issue.</p>
                                </td>
                            </tr>
                            
                            <tr class="row">
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__1">Test the state of master mirroring on Greenplum Database. If
                                    the value is not "STREAMING", raise an alert or
                                        warning.<p class="p">Recommended frequency: run every 5 to 10
                                        minutes</p><p class="p">Severity: IMPORTANT</p></td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__2">
                                    <div class="p">Run the following <code class="ph codeph">psql</code>
                                        command:<pre class="pre codeblock"><code>psql &lt;dbname&gt; -c 'SELECT pid, state FROM pg_stat_replication;'</code></pre></div>
                                </td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__3">
                                    <p class="p">Check the <span class="ph filepath">log</span> file from the master
                                        and standby master for errors. If there are no unexpected
                                        errors and the machines are up, run the
                                            <code class="ph codeph">gpinitstandby</code> utility to bring the
                                        standby online. </p>
                                </td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__1">Perform a basic check to see if the master is up and
                                        functioning.<p class="p">Recommended frequency: run every 5 to 10
                                        minutes</p><p class="p">Severity: CRITICAL</p></td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__2">
                                    <div class="p">Run the following query in the <code class="ph codeph">postgres</code>
                                        database:
                                        <pre class="pre codeblock"><code>SELECT count(*) FROM gp_segment_configuration;</code></pre></div>
                                </td>
                                <td class="entry" headers="drr_5bg_rp__table_drr_5bg_rp__entry__3">
                                    <p class="p">If this query fails, the active master may be down. Try to
                                        start the database on the original master if the server is
                                        up and running. If that fails, try to activate the standby
                                        master as master.</p>
                                </td>
                            </tr>
                        </tbody></table>

## <a id="topic_y4c_4gg_rp"></a>Hardware and Operating System Monitoring 

<table class="table frame-all" id="topic_y4c_4gg_rp__table_ls5_sgg_rp"><caption><span class="table--title-label">Table 2. </span><span class="title">Hardware and Operating System Monitoring Activities</span></caption><colgroup><col><col><col></colgroup><thead class="thead">
                            <tr class="row">
                                <th class="entry" id="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__1">Activity</th>
                                <th class="entry" id="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__2">Procedure</th>
                                <th class="entry" id="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__3">Corrective Actions</th>
                            </tr>
                        </thead><tbody class="tbody">
                            <tr class="row">
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__1">Check disk space usage on volumes used for Greenplum Database data storage and the OS.
                                        <p class="p">Recommended frequency: every 5 to 30
                                        minutes</p><p class="p">Severity: CRITICAL</p></td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__2">
                                    <div class="p">Set up a disk space check.<ul class="ul" id="topic_y4c_4gg_rp__ul_rs1_whg_rp">
                                            <li class="li">Set a threshold to raise an alert when a disk
                                                reaches a percentage of capacity. The recommended
                                                threshold is 75% full.</li>
                                            <li class="li">It is not recommended to run the system with
                                                capacities approaching 100%.</li>
                                        </ul></div>
                                </td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__3">Use <code class="ph codeph">VACUUM</code>/<code class="ph codeph">VACUUM FULL</code> on
                                    user tables to reclaim space occupied by dead rows.</td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__1">Check for errors or dropped packets on the network
                                        interfaces.<p class="p">Recommended frequency: hourly</p><p class="p">Severity:
                                        IMPORTANT</p></td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__2">Set up a network interface checks. </td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__3">
                                    <p class="p">Work with network and OS teams to resolve errors.</p>
                                </td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__1">Check for RAID errors or degraded RAID performance.
                                        <p class="p">Recommended frequency: every 5 minutes</p><p class="p">Severity:
                                        CRITICAL</p></td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__2">Set up a RAID check.</td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__3">
                                    <ul class="ul" id="topic_y4c_4gg_rp__ul_f4b_fjg_rp">
                                        <li class="li">Replace failed disks as soon as possible. </li>
                                        <li class="li">Work with system administration team to resolve other
                                            RAID or controller errors as soon as possible.</li>
                                    </ul>
                                </td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__1">Check for adequate I/O bandwidth and I/O skew.<p class="p">Recommended
                                        frequency: when create a cluster or when hardware issues are
                                        suspected.</p></td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__2">Run the Greenplum
                                    <code class="ph codeph">gpcheckperf</code> utility. </td>
                                <td class="entry" headers="topic_y4c_4gg_rp__table_ls5_sgg_rp__entry__3">
                                    <div class="p">The cluster may be under-specified if data transfer rates are
                                        not similar to the following: <ul class="ul" id="topic_y4c_4gg_rp__ul_elm_dkg_rp">
                                            <li class="li">2GB per second disk read</li>
                                            <li class="li">1 GB per second disk write</li>
                                            <li class="li">10 Gigabit per second network read and write </li>
                                        </ul>If transfer rates are lower than expected, consult with
                                        your data architect regarding performance expectations.</div>
                                    <p class="p">If the machines on the cluster display an uneven performance
                                        profile, work with the system administration team to fix
                                        faulty machines.</p>
                                </td>
                            </tr>
                        </tbody></table>

## <a id="topic_gbp_jng_rp"></a>Catalog Monitoring 

<table class="table frame-all" id="topic_gbp_jng_rp__table_pdq_lng_rp"><caption><span class="table--title-label">Table 3. </span><span class="title">Catalog Monitoring Activities</span></caption><colgroup><col><col><col></colgroup><thead class="thead">
                            <tr class="row">
                                <th class="entry" id="topic_gbp_jng_rp__table_pdq_lng_rp__entry__1">Activity</th>
                                <th class="entry" id="topic_gbp_jng_rp__table_pdq_lng_rp__entry__2">Procedure</th>
                                <th class="entry" id="topic_gbp_jng_rp__table_pdq_lng_rp__entry__3">Corrective Actions</th>
                            </tr>
                        </thead><tbody class="tbody">
                            <tr class="row">
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__1">Run catalog consistency checks in each database to ensure the
                                    catalog on each host in the cluster is consistent and in a good
                                        state.<p class="p">You may run this command while the database is up
                                        and running. </p><p class="p">Recommended frequency:
                                        weekly</p><p class="p">Severity: IMPORTANT</p></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__2">Run the Greenplum <code class="ph codeph">gpcheckcat</code> utility in each
                                        database:<pre class="pre codeblock"><code>gpcheckcat -O</code></pre><div class="note note note_note"><span class="note__title">Note:</span>  With the
                                            <code class="ph codeph">-O</code> option, <code class="ph codeph">gpcheckcat</code>
                                        runs just 10 of its usual 15 tests.</div></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__3">Run the repair scripts for any issues identified.</td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__1">Check for <code class="ph codeph">pg_class</code> entries that have no
                                    corresponding pg_<code class="ph codeph">attribute</code> entry.<p class="p">Recommended
                                        frequency: monthly</p><p class="p">Severity: IMPORTANT</p></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__2">During a downtime, with no users on the system, run the
                                        Greenplum
                                    <code class="ph codeph">gpcheckcat</code> utility in each
                                    database:<pre class="pre codeblock"><code>gpcheckcat -R pgclass</code></pre></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__3">Run the repair scripts for any issues identified.</td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__1">Check for leaked temporary schema and missing schema
                                        definition.<p class="p">Recommended frequency: monthly</p><p class="p">Severity:
                                        IMPORTANT</p></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__2">During a downtime, with no users on the system, run the
                                        Greenplum
                                    <code class="ph codeph">gpcheckcat</code> utility in each
                                    database:<pre class="pre codeblock"><code>gpcheckcat -R namespace</code></pre></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__3">Run the repair scripts for any issues identified.</td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__1">Check constraints on randomly distributed
                                        tables.<p class="p">Recommended frequency: monthly</p><p class="p">Severity:
                                        IMPORTANT</p></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__2">During a downtime, with no users on the system, run the
                                        Greenplum
                                    <code class="ph codeph">gpcheckcat</code> utility in each
                                    database:<pre class="pre codeblock"><code>gpcheckcat -R distribution_policy</code></pre></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__3">Run the repair scripts for any issues identified.</td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__1">Check for dependencies on non-existent objects.<p class="p">Recommended
                                        frequency: monthly</p><p class="p">Severity: IMPORTANT</p></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__2">During a downtime, with no users on the system, run the
                                        Greenplum
                                    <code class="ph codeph">gpcheckcat</code> utility in each
                                    database:<pre class="pre codeblock"><code>gpcheckcat -R dependency</code></pre></td>
                                <td class="entry" headers="topic_gbp_jng_rp__table_pdq_lng_rp__entry__3">Run the repair scripts for any issues identified.</td>
                            </tr>
                        </tbody></table>

## <a id="maintentenance_check_scripts"></a>Data Maintenance 

<table class="table frame-all" id="maintentenance_check_scripts__table_tp4_nxg_rp"><caption><span class="table--title-label">Table 4. </span><span class="title">Data Maintenance Activities</span></caption><colgroup><col><col><col></colgroup><thead class="thead">
                            <tr class="row">
                                <th class="entry" id="maintentenance_check_scripts__table_tp4_nxg_rp__entry__1">Activity</th>
                                <th class="entry" id="maintentenance_check_scripts__table_tp4_nxg_rp__entry__2">Procedure</th>
                                <th class="entry" id="maintentenance_check_scripts__table_tp4_nxg_rp__entry__3">Corrective Actions</th>
                            </tr>
                        </thead><tbody class="tbody">
                            <tr class="row">
                                <td class="entry" headers="maintentenance_check_scripts__table_tp4_nxg_rp__entry__1">Check for missing statistics on tables. </td>
                                <td class="entry" headers="maintentenance_check_scripts__table_tp4_nxg_rp__entry__2">Check the <code class="ph codeph">gp_stats_missing</code> view in each
                                    database:<pre class="pre codeblock"><code>SELECT * FROM gp_toolkit.gp_stats_missing;</code></pre></td>
                                <td class="entry" headers="maintentenance_check_scripts__table_tp4_nxg_rp__entry__3">Run <code class="ph codeph">ANALYZE</code> on tables that are missing
                                    statistics.</td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="maintentenance_check_scripts__table_tp4_nxg_rp__entry__1">Check for tables that have bloat (dead space) in data files
                                    that cannot be recovered by a regular <code class="ph codeph">VACUUM</code>
                                    command. <p class="p">Recommended frequency: weekly or
                                        monthly</p><p class="p">Severity: WARNING</p></td>
                                <td class="entry" headers="maintentenance_check_scripts__table_tp4_nxg_rp__entry__2">Check the <code class="ph codeph">gp_bloat_diag</code> view in each
                                    database:
                                    <pre class="pre codeblock"><code>SELECT * FROM gp_toolkit.gp_bloat_diag;</code></pre></td>
                                <td class="entry" headers="maintentenance_check_scripts__table_tp4_nxg_rp__entry__3"><code class="ph codeph">VACUUM FULL</code> acquires an <code class="ph codeph">ACCESS
                                        EXCLUSIVE</code> lock on tables. Run <code class="ph codeph">VACUUM
                                        FULL</code> during a time when users and applications do
                                    not require access to the tables, such as during a time of low
                                    activity, or during a maintenance window.</td>
                            </tr>
                        </tbody></table>

## <a id="topic_dld_23h_rp"></a>Database Maintenance 

<table class="table frame-all" id="topic_dld_23h_rp__table_vxx_f3h_rp"><caption><span class="table--title-label">Table 5. </span><span class="title">Database Maintenance Activities</span></caption><colgroup><col><col><col></colgroup><thead class="thead">
                                <tr class="row">
                                    <th class="entry" id="topic_dld_23h_rp__table_vxx_f3h_rp__entry__1">Activity</th>
                                    <th class="entry" id="topic_dld_23h_rp__table_vxx_f3h_rp__entry__2">Procedure</th>
                                    <th class="entry" id="topic_dld_23h_rp__table_vxx_f3h_rp__entry__3">Corrective Actions</th>
                                </tr>
                            </thead><tbody class="tbody">
                                <tr class="row">
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__1">Reclaim space occupied by deleted rows in the  heap
                                        tables so that the space they occupy can be
                                            reused.<p class="p">Recommended frequency: daily</p><p class="p">Severity:
                                            CRITICAL</p></td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__2">Vacuum user
                                        tables:<pre class="pre codeblock"><code>VACUUM &lt;table&gt;;</code></pre></td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__3">Vacuum updated tables regularly to prevent bloating.
                                    </td>
                                </tr>
                                <tr class="row">
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__1">Update table statistics. <p class="p">Recommended frequency: after
                                            loading data and before executing
                                            queries</p><p class="p">Severity: CRITICAL</p></td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__2">Analyze user tables. You can use the
                                            <code class="ph codeph">analyzedb</code> management
                                        utility:<pre class="pre codeblock"><code>analyzedb -d &lt;database&gt; -a</code></pre></td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__3">Analyze updated tables regularly so that the optimizer
                                        can produce efficient query execution plans.</td>
                                </tr>
                                <tr class="row">
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__1">Backup the database data.<p class="p">Recommended frequency: daily,
                                            or as required by your backup plan</p><p class="p">Severity:
                                            CRITICAL</p></td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__2">
                                        <span class="ph">Run the <code class="ph codeph">gpbackup</code>
                                            utility to create a backup of the master and segment
                                            databases in parallel.</span>
                                    </td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__3">Best practice is to have a current backup ready in case
                                        the database must be restored. </td>
                                </tr>
                                <tr class="row">
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__1">Vacuum, reindex, and analyze system catalogs to maintain
                                        an efficient catalog.<p class="p">Recommended frequency: weekly, or
                                            more often if database objects are created and dropped
                                            frequently</p></td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__2">
                                        <ol class="ol" id="topic_dld_23h_rp__ol_frs_nlh_rp">
                                            <li class="li"><code class="ph codeph">VACUUM</code> the system tables in each
                                                database.</li>
                                            <li class="li">Run <code class="ph codeph">REINDEX SYSTEM</code> in each
                                                database, or use the <code class="ph codeph">reindexdb</code>
                                                command-line utility with the <code class="ph codeph">-s</code>
                                                option:
                                                <pre class="pre codeblock"><code>reindexdb -s &lt;database&gt;</code></pre></li>
                                            <li class="li"><code class="ph codeph">ANALYZE</code> each of the system
                                                tables:<pre class="pre codeblock"><code>analyzedb -s pg_catalog -d &lt;database&gt;</code></pre></li>
                                        </ol>
                                    </td>
                                    <td class="entry" headers="topic_dld_23h_rp__table_vxx_f3h_rp__entry__3">The optimizer retrieves information from the system
                                        tables to create query plans. If system tables and indexes
                                        are allowed to become bloated over time, scanning the system
                                        tables increases query execution time. It is important to
                                        run <code class="ph codeph">ANALYZE</code> after reindexing, because
                                            <code class="ph codeph">REINDEX</code> leaves indexes with no
                                        statistics. </td>
                                </tr>
                            </tbody></table>

## <a id="topic_idx_smh_rp"></a>Patching and Upgrading 

<table class="table frame-all" id="topic_idx_smh_rp__table_td5_5mh_rp"><caption><span class="table--title-label">Table 6. </span><span class="title">Patch and Upgrade Activities</span></caption><colgroup><col style="width:33.003300330033%"><col style="width:33.66336633663366%"><col style="width:33.33333333333333%"></colgroup><thead class="thead">
                            <tr class="row">
                                <th class="entry" id="topic_idx_smh_rp__table_td5_5mh_rp__entry__1">Activity</th>
                                <th class="entry" id="topic_idx_smh_rp__table_td5_5mh_rp__entry__2">Procedure</th>
                                <th class="entry" id="topic_idx_smh_rp__table_td5_5mh_rp__entry__3">Corrective Actions</th>
                            </tr>
                        </thead><tbody class="tbody">
                            <tr class="row">
                                <td class="entry" headers="topic_idx_smh_rp__table_td5_5mh_rp__entry__1">Ensure any bug fixes or enhancements are applied to the
                                        kernel.<p class="p">Recommended frequency: at least every 6
                                        months</p><p class="p">Severity: IMPORTANT</p></td>
                                <td class="entry" headers="topic_idx_smh_rp__table_td5_5mh_rp__entry__2">Follow the vendor's instructions to update the Linux
                                    kernel.</td>
                                <td class="entry" headers="topic_idx_smh_rp__table_td5_5mh_rp__entry__3">Keep the kernel current to include bug fixes and security
                                    fixes, and to avoid difficult future upgrades. </td>
                            </tr>
                            <tr class="row">
                                <td class="entry" headers="topic_idx_smh_rp__table_td5_5mh_rp__entry__1">Install Greenplum Database minor releases<span class="ph">, for example
                                            5.0.<em class="ph i">x</em></span>.<p class="p">Recommended frequency:
                                        quarterly</p><p class="p">Severity: IMPORTANT</p></td>
                                <td class="entry" headers="topic_idx_smh_rp__table_td5_5mh_rp__entry__2">Follow upgrade instructions in the Greenplum Database
                                    <em class="ph i">Release Notes</em>. Always upgrade to the latest in the
                                    series.</td>
                                <td class="entry" headers="topic_idx_smh_rp__table_td5_5mh_rp__entry__3">Keep the Greenplum Database software current to
                                    incorporate bug fixes, performance enhancements, and feature
                                    enhancements into your Greenplum
                                    cluster. </td>
                            </tr>
                        </tbody></table>
