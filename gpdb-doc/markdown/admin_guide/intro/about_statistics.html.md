---
title: About Database Statistics in Greenplum Database 
---

An overview of statistics gathered by the ANALYZE command in Greenplum Database.

Statistics are metadata that describe the data stored in the database. The query optimizer needs up-to-date statistics to choose the best execution plan for a query. For example, if a query joins two tables and one of them must be broadcast to all segments, the optimizer can choose the smaller of the two tables to minimize network traffic.

The statistics used by the optimizer are calculated and saved in the system catalog by the `ANALYZE` command. There are three ways to initiate an analyze operation:

-   You can run the `ANALYZE` command directly.
-   You can run the `analyzedb` management utility outside of the database, at the command line.
-   An automatic analyze operation can be triggered when DML operations are performed on tables that have no statistics or when a DML operation modifies a number of rows greater than a specified threshold.

These methods are described in the following sections. The `VACUUM ANALYZE` command is another way to initiate an analyze operation, but its use is discouraged because vacuum and analyze are different operations with different purposes.

Calculating statistics consumes time and resources, so Greenplum Database produces estimates by calculating statistics on samples of large tables. In most cases, the default settings provide the information needed to generate correct execution plans for queries. If the statistics produced are not producing optimal query execution plans, the administrator can tune configuration parameters to produce more accurate statistics by increasing the sample size or the granularity of statistics saved in the system catalog. Producing more accurate statistics has CPU and storage costs and may not produce better plans, so it is important to view explain plans and test query performance to ensure that the additional statistics-related costs result in better query performance.

**Parent topic:** [Greenplum Database Concepts](../intro/partI.html)

## <a id="topic_oq3_qxj_3s"></a>System Statistics 

### <a id="tabsize"></a>Table Size 

The query planner seeks to minimize the disk I/O and network traffic required to run a query, using estimates of the number of rows that must be processed and the number of disk pages the query must access. The data from which these estimates are derived are the `pg_class` system table columns `reltuples` and `relpages`, which contain the number of rows and pages at the time a `VACUUM` or `ANALYZE` command was last run. As rows are added or deleted, the numbers become less accurate. However, an accurate count of disk pages is always available from the operating system, so as long as the ratio of `reltuples` to `relpages` does not change significantly, the optimizer can produce an estimate of the number of rows that is sufficiently accurate to choose the correct query execution plan.

When the `reltuples` column differs significantly from the row count returned by `SELECT COUNT(*)`, an analyze should be performed to update the statistics.

When a `REINDEX` command finishes recreating an index, the `relpages` and `reltuples` columns are set to zero. The `ANALYZE` command should be run on the base table to update these columns.

### <a id="pgstattab"></a>The pg\_statistic System Table and pg\_stats View 

The `pg_statistic` system table holds the results of the last `ANALYZE` operation on each database table. There is a row for each column of every table. It has the following columns:

starelid
:   The object ID of the table or index the column belongs to.

staattnum
:   The number of the described column, beginning with 1.

stainherit
:   If true, the statistics include inheritance child columns, not just the values in the specified relation.

stanullfrac
:   The fraction of the column's entries that are null.

stawidth
:   The average stored width, in bytes, of non-null entries.

stadistinct
:   A positive number is an estimate of the number of distinct values in the column; the number is not expected to vary with the number of rows. A negative value is the number of distinct values divided by the number of rows, that is, the ratio of rows with distinct values for the column, negated. This form is used when the number of distinct values increases with the number of rows. A unique column, for example, has an `n_distinct` value of -1.0. Columns with an average width greater than 1024 are considered unique.

stakind<i>N</i>
:   A code number indicating the kind of statistics stored in the <i>N</i>th slot of the `pg_statistic` row.

staop<i>N</i>
:   An operator used to derive the statistics stored in the <i>N</i>th slot. For example, a histogram slot would show the < operator that defines the sort order of the data.

stanumbers<i>N</i>
:   float4 array containing numerical statistics of the appropriate kind for the <i>N</i>th slot, or `NULL` if the slot kind does not involve numerical values.

stavalues<i>N</i>
:   Column data values of the appropriate kind for the <i>N</i>th slot, or `NULL` if the slot kind does not store any data values. Each array's element values are actually of the specific column's data type, so there is no way to define these columns' types more specifically than <i>anyarray</i>.

The statistics collected for a column vary for different data types, so the `pg_statistic` table stores statistics that are appropriate for the data type in four <i>slots</i>, consisting of four columns per slot. For example, the first slot, which normally contains the most common values for a column, consists of the columns `stakind1`, `staop1`, `stanumbers1`, and `stavalues1`.

The `stakindN` columns each contain a numeric code to describe the type of statistics stored in their slot. The `stakind` code numbers from 1 to 99 are reserved for core PostgreSQL data types. Greenplum Database uses code numbers 1, 2, 3, 4, 5, and 99. A value of 0 means the slot is unused. The following table describes the kinds of statistics stored for the three codes.

<table class="table frame-all" id="topic_oq3_qxj_3s__table_upf_1yc_nt"><caption><span class="table--title-label">Table 1. </span><span class="title">Contents of pg_statistic "slots"</span></caption><colgroup><col style="width:14.285714285714285%"><col style="width:85.71428571428571%"></colgroup><thead class="thead">
                <tr class="row">
                  <th class="entry" id="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__1">stakind Code</th>
                  <th class="entry" id="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__2">Description</th>
                </tr>
              </thead><tbody class="tbody">
                <tr class="row">
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__1">1</td>
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__2"><em class="ph i">Most CommonValues (MCV) Slot</em>
                    <ul class="ul" id="topic_oq3_qxj_3s__ul_ipg_gyc_nt">
                      <li class="li"><code class="ph codeph">staop</code> contains the object ID of the "=" operator, used to
                        decide whether values are the same or not.</li>
                      <li class="li"><code class="ph codeph">stavalues</code> contains an array of the <var class="keyword varname">K</var>
                        most common non-null values appearing in the column.</li>
                      <li class="li"><code class="ph codeph">stanumbers</code> contains the frequencies (fractions of total
                        row count) of the values in the <code class="ph codeph">stavalues</code> array. </li>
                    </ul>The values are ordered in decreasing frequency. Since the arrays are
                    variable-size, <var class="keyword varname">K</var> can be chosen by the statistics collector.
                    Values must occur more than once to be added to the <code class="ph codeph">stavalues</code>
                    array; a unique column has no MCV slot.</td>
                </tr>
                <tr class="row">
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__1">2</td>
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__2"><em class="ph i">Histogram Slot</em> – describes the distribution of scalar data.<ul class="ul" id="topic_oq3_qxj_3s__ul_t2f_zyc_nt">
                      <li class="li"><code class="ph codeph">staop</code> is the object ID of the "&lt;" operator, which
                        describes the sort ordering. </li>
                      <li class="li"><code class="ph codeph">stavalues</code> contains <var class="keyword varname">M</var> (where
                            <code class="ph codeph"><var class="keyword varname">M</var>&gt;=2</code>) non-null values that divide
                        the non-null column data values into <code class="ph codeph"><var class="keyword varname">M</var>-1</code>
                        bins of approximately equal population. The first <code class="ph codeph">stavalues</code>
                        item is the minimum value and the last is the maximum value. </li>
                      <li class="li"><code class="ph codeph">stanumbers</code> is not used and should be
                          <code class="ph codeph">NULL</code>. </li>
                    </ul><p class="p">If a Most Common Values slot is also provided, then the histogram
                      describes the data distribution after removing the values listed in the MCV
                      array. (It is a <em class="ph i">compressed histogram</em> in the technical parlance). This
                      allows a more accurate representation of the distribution of a column with
                      some very common values. In a column with only a few distinct values, it is
                      possible that the MCV list describes the entire data population; in this case
                      the histogram reduces to empty and should be omitted.</p></td>
                </tr>
                <tr class="row">
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__1">3</td>
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__2"><em class="ph i">Correlation Slot</em> – describes the correlation between the physical
                    order of table tuples and the ordering of data values of this column. <ul class="ul" id="topic_oq3_qxj_3s__ul_yvj_sfd_nt">
                      <li class="li"><code class="ph codeph">staop</code> is the object ID of the "&lt;" operator. As with
                        the histogram, more than one entry could theoretically appear.</li>
                      <li class="li"><code class="ph codeph">stavalues</code> is not used and should be
                        <code class="ph codeph">NULL</code>. </li>
                      <li class="li"><code class="ph codeph">stanumbers</code> contains a single entry, the correlation
                        coefficient between the sequence of data values and the sequence of their
                        actual tuple positions. The coefficient ranges from +1 to -1.</li>
                    </ul></td>
                </tr>
                <tr class="row">
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__1">4</td>
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__2"><em class="ph i">Most Common Elements Slot</em> - is similar to a Most Common Values (MCV)
                    Slot, except that it stores the most common non-null <em class="ph i">elements</em> of the
                    column values. This is useful when the column datatype is an array or some other
                    type with identifiable elements (for instance, <code class="ph codeph">tsvector</code>). <ul class="ul" id="topic_oq3_qxj_3s__ul_kj4_wnm_y2b">
                      <li class="li"><code class="ph codeph">staop</code> contains the equality operator appropriate to the
                        element type. </li>
                      <li class="li"><code class="ph codeph">stavalues</code> contains the most common element values.</li>
                      <li class="li"><code class="ph codeph">stanumbers</code> contains common element frequencies. </li>
                    </ul><p class="p">Frequencies are measured as the fraction of non-null rows the element
                      value appears in, not the frequency of all rows. Also, the values are sorted
                      into the element type's default order (to support binary search for a
                      particular value). Since this puts the minimum and maximum frequencies at
                      unpredictable spots in <code class="ph codeph">stanumbers</code>, there are two extra
                      members of <code class="ph codeph">stanumbers</code> that hold copies of the minimum and
                      maximum frequencies. Optionally, there can be a third extra member that holds
                      the frequency of null elements (the frequency is expressed in the same terms:
                      the fraction of non-null rows that contain at least one null element). If this
                      member is omitted, the column is presumed to contain no <code class="ph codeph">NULL</code>
                      elements. </p>
                    <div class="note note note_note"><span class="note__title">Note:</span> For <code class="ph codeph">tsvector</code> columns, the <code class="ph codeph">stavalues</code>
                      elements are of type <code class="ph codeph">text</code>, even though their representation
                      within <code class="ph codeph">tsvector</code> is not exactly
                    <code class="ph codeph">text</code>.</div></td>
                </tr>
                <tr class="row">
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__1">5</td>
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__2"><em class="ph i">Distinct Elements Count Histogram Slot</em> - describes the distribution
                    of the number of distinct element values present in each row of an array-type
                    column. Only non-null rows are considered, and only non-null elements.<ul class="ul" id="topic_oq3_qxj_3s__ul_gmr_jnm_y2b">
                      <li class="li"><code class="ph codeph">staop</code> contains the equality operator appropriate to the
                        element type. </li>
                      <li class="li"><code class="ph codeph">stavalues</code> is not used and should be
                        <code class="ph codeph">NULL</code>. </li>
                      <li class="li"><code class="ph codeph">stanumbers</code> contains information about distinct elements.
                        The last member of <code class="ph codeph">stanumbers</code> is the average count of
                        distinct element values over all non-null rows. The preceding
                          <var class="keyword varname">M</var> (where <code class="ph codeph"><var class="keyword varname">M</var> &gt;=2</code>)
                        members form a histogram that divides the population of distinct-elements
                        counts into <code class="ph codeph"><var class="keyword varname">M</var>-1</code> bins of approximately
                        equal population. The first of these is the minimum observed count, and the
                        last the maximum.</li>
                    </ul></td>
                </tr>
                <tr class="row">
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__1">99</td>
                  <td class="entry" headers="topic_oq3_qxj_3s__table_upf_1yc_nt__entry__2"><em class="ph i">Hyperloglog Slot</em> - for child leaf partitions of a partitioned table,
                    stores the <code class="ph codeph">hyperloglog_counter</code> created for the sampled data.
                    The <code class="ph codeph">hyperloglog_counter</code> data structure is converted into a
                      <code class="ph codeph">bytea</code> and stored in a <code class="ph codeph">stavalues5</code> slot of the
                      <code class="ph codeph">pg_statistic</code> catalog table.</td>
                </tr>
              </tbody></table>

The `pg_stats` view presents the contents of `pg_statistic` in a friendlier format. The `pg_stats` view has the following columns:

schemaname
:   The name of the schema containing the table.

tablename
:   The name of the table.

attname
:   The name of the column this row describes.

inherited
:   If true, the statistics include inheritance child columns.

null\_frac
:   The fraction of column entries that are null.

avg\_width
:   The average storage width in bytes of the column's entries, calculated as `avg(pg_column_size(column_name))`.

n\_distinct
:   A positive number is an estimate of the number of distinct values in the column; the number is not expected to vary with the number of rows. A negative value is the number of distinct values divided by the number of rows, that is, the ratio of rows with distinct values for the column, negated. This form is used when the number of distinct values increases with the number of rows. A unique column, for example, has an `n_distinct` value of -1.0. Columns with an average width greater than 1024 are considered unique.

most\_common\_vals
:   An array containing the most common values in the column, or null if no values seem to be more common. If the `n_distinct` column is -1, `most_common_vals` is null. The length of the array is the lesser of the number of actual distinct column values or the value of the `default_statistics_target` configuration parameter. The number of values can be overridden for a column using `ALTER TABLE table SET COLUMN column SET STATISTICS N`.

most\_common\_freqs
:   An array containing the frequencies of the values in the `most_common_vals` array. This is the number of occurrences of the value divided by the total number of rows. The array is the same length as the `most_common_vals` array. It is null if `most_common_vals` is null.

histogram\_bounds
:   An array of values that divide the column values into groups of approximately the same size. A histogram can be defined only if there is a `max()` aggregate function for the column. The number of groups in the histogram is the same as the `most_common_vals` array size.

correlation
:   Greenplum Database computes correlation statistics for both heap and AO/AOCO tables, but the Postgres Planner uses these statistics only for heap tables.

most\_common\_elems
:   An array that contains the most common element values.

most\_common\_elem\_freqs
:   An array that contains common element frequencies.

elem\_count\_histogram
:   An array that describes the distribution of the number of distinct element values present in each row of an array-type column.

Newly created tables and indexes have no statistics. You can check for tables with missing statistics using the `gp_stats_missing` view, which is in the `gp_toolkit` schema:

```
SELECT * from gp_toolkit.gp_stats_missing;
```

### <a id="section_wsy_1rv_mt"></a>Sampling 

When calculating statistics for large tables, Greenplum Database creates a smaller table by sampling the base table. If the table is partitioned, samples are taken from all partitions.

### <a id="section_u5p_brv_mt"></a>Updating Statistics 

Running `ANALYZE` with no arguments updates statistics for all tables in the database. This could take a very long time, so it is better to analyze tables selectively after data has changed. You can also analyze a subset of the columns in a table, for example columns used in joins, `WHERE` clauses, `SORT` clauses, `GROUP BY` clauses, or `HAVING` clauses.

Analyzing a severely bloated table can generate poor statistics if the sample contains empty pages, so it is good practice to vacuum a bloated table before analyzing it.

See the *SQL Command Reference* in the *Greenplum Database Reference Guide* for details of running the `ANALYZE` command.

Refer to the *Greenplum Database Management Utility Reference* for details of running the `analyzedb` command.

### <a id="section_cv2_crv_mt"></a>Analyzing Partitioned Tables 

When the `ANALYZE` command is run on a partitioned table, it analyzes each child leaf partition table, one at a time. You can run `ANALYZE` on just new or changed partition tables to avoid analyzing partitions that have not changed.

The `analyzedb` command-line utility skips unchanged partitions automatically. It also runs concurrent sessions so it can analyze several partitions concurrently. It runs five sessions by default, but the number of sessions can be set from 1 to 10 with the `-p` command-line option. Each time `analyzedb` runs, it saves state information for append-optimized tables and partitions in the `db_analyze` directory in the master data directory. The next time it runs, `analyzedb` compares the current state of each table with the saved state and skips analyzing a table or partition if it is unchanged. Heap tables are always analyzed.

If GPORCA is enabled \(the default\), you also need to run `ANALYZE` or `ANALYZE ROOTPARTITION` on the root partition of a partitioned table \(not a leaf partition\) to refresh the root partition statistics. GPORCA requires statistics at the root level for partitioned tables. The Postgres Planner does not use these statistics.

The time to analyze a partitioned table is similar to the time to analyze a non-partitioned table since `ANALYZE ROOTPARTITION` does not collect statistics on the leaf partitions (the data is only sampled).

The Greenplum Database server configuration parameter [optimizer\_analyze\_root\_partition](../../ref_guide/config_params/guc-list.html) affects when statistics are collected on the root partition of a partitioned table. If the parameter is `on` \(the default\), the `ROOTPARTITION` keyword is not required to collect statistics on the root partition when you run `ANALYZE`. Root partition statistics are collected when you run `ANALYZE` on the root partition, or when you run `ANALYZE` on a child leaf partition of the partitioned table and the other child leaf partitions have statistics. If the parameter is `off`, you must run `ANALYZE ROOTPARTITION` to collect root partition statistics.

If you do not intend to run queries on partitioned tables with GPORCA \(setting the server configuration parameter [optimizer](../../ref_guide/config_params/guc-list.html) to `off`\), you can also set the server configuration parameter `optimizer_analyze_root_partition` to `off` to limit when `ANALYZE` updates the root partition statistics.

## <a id="topic_gyb_qrd_2t"></a>Configuring Statistics 

There are several options for configuring Greenplum Database statistics collection.

### <a id="stattarg"></a>Statistics Target 

The statistics target is the size of the `most_common_vals`, `most_common_freqs`, and `histogram_bounds` arrays for an individual column. By default, the target is 25. The default target can be changed by setting a server configuration parameter and the target can be set for any column using the `ALTER TABLE` command. Larger values increase the time needed to do `ANALYZE`, but may improve the quality of the Postgres Planner estimates.

Set the system default statistics target to a different value by setting the `default_statistics_target` server configuration parameter. The default value is usually sufficient, and you should only raise or lower it if your tests demonstrate that query plans improve with the new target. For example, to raise the default statistics target from 100 to 150 you can use the `gpconfig` utility:

```
gpconfig -c default_statistics_target -v 150
```

The statistics target for individual columns can be set with the `ALTER TABLE` command. For example, some queries can be improved by increasing the target for certain columns, especially columns that have irregular distributions. You can set the target to zero for columns that never contribute to query optimization. When the target is 0, `ANALYZE` ignores the column. For example, the following `ALTER TABLE` command sets the statistics target for the `notes` column in the `emp` table to zero:

```
ALTER TABLE emp ALTER COLUMN notes SET STATISTICS 0;
```

The statistics target can be set in the range 0 to 1000, or set it to -1 to revert to using the system default statistics target.

Setting the statistics target on a parent partition table affects the child partitions. If you set statistics to 0 on some columns on the parent table, the statistics for the same columns are set to 0 for all children partitions. However, if you later add or exchange another child partition, the new child partition will use either the default statistics target or, in the case of an exchange, the previous statistics target. Therefore, if you add or exchange child partitions, you should set the statistics targets on the new child table.

### <a id="section_j3p_drv_mt"></a>Automatic Statistics Collection 

Greenplum Database can be set to automatically run `ANALYZE` on a table that either has no statistics or has changed significantly when certain operations are performed on the table. For partitioned tables, automatic statistics collection is only triggered when the operation is run directly on a leaf table, and then only the leaf table is analyzed.

Automatic statistics collection is governed by a server configuration parameter, and has three modes:

-   `none` deactivates automatic statistics collection.
-   `on_no_stats` triggers an analyze operation for a table with no existing statistics when any of the commands `CREATE TABLE AS SELECT`, `INSERT`, or `COPY` are run on the table by the table owner.
-   `on_change` triggers an analyze operation when any of the commands `CREATE TABLE AS SELECT`, `UPDATE`, `DELETE`, `INSERT`, or `COPY` are run on the table by the table owner, and the number of rows affected exceeds the threshold defined by the `gp_autostats_on_change_threshold` configuration parameter.

The automatic statistics collection mode is set separately for commands that occur within a procedural language function and commands that run outside of a function:

-   The `gp_autostats_mode` configuration parameter controls automatic statistics collection behavior outside of functions and is set to `on_no_stats` by default.
-   The `gp_autostats_mode_in_functions` parameter controls the behavior when table operations are performed within a procedural language function and is set to `none` by default.

With the `on_change` mode, `ANALYZE` is triggered only if the number of rows affected exceeds the threshold defined by the `gp_autostats_on_change_threshold` configuration parameter. The default value for this parameter is a very high value, 2147483647, which effectively deactivates automatic statistics collection; you must set the threshold to a lower number to enable it. The `on_change` mode could trigger large, unexpected analyze operations that could disrupt the system, so it is not recommended to set it globally. It could be useful in a session, for example to automatically analyze a table following a load.

Setting the `gp_autostats_allow_nonowner` server configuration parameter to `true` also instructs Greenplum Database to trigger automatic statistics collection on a table when:

-   `gp_autostats_mode=on_change` and the table is modified by a non-owner.
-   `gp_autostats_mode=on_no_stats` and the first user to `INSERT` or `COPY` into the table is a non-owner.

To deactivate automatic statistics collection outside of functions, set the `gp_autostats_mode` parameter to `none`:

```
gpconfigure -c gp_autostats_mode -v none
```

To enable automatic statistics collection in functions for tables that have no statistics, change `gp_autostats_mode_in_functions` to `on_no_stats`:

```
gpconfigure -c gp_autostats_mode_in_functions -v on_no_stats
```

Set the `log_autostats` system configuration parameter to on if you want to log automatic statistics collection operations.

