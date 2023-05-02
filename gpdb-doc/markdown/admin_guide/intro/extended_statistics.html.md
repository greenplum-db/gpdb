---
title: About Single-Column and Extended Statistics
---

The query planner estimates the number of rows retrieved by a query in order to make good choices of query plans. Most queries retrieve only a fraction of the rows in a table, due to `WHERE` clauses that restrict the rows to be examined. The planner thus needs to make an estimate of the selectivity of `WHERE` clauses, that is, the fraction of rows that match each condition in the `WHERE` clause. The information used for this task is stored in the [pg_statistic](../../ref_guide/system_catalogs/pg_statistic.html) system catalog. Entries in `pg_statistic` are updated by the `ANALYZE` and `VACUUM ANALYZE` commands, and are always approximate even when freshly updated.

The amount of information stored in `pg_statistic` by `ANALYZE`, in particular the maximum number of entries in the `most_common_vals` and `histogram_bounds` arrays for each column, can be set on a column-by-column basis using the `ALTER TABLE SET STATISTICS` command, or globally by setting the [default_statistics_target](../../ref_guide/config_params/guc-list.html#default_statistics_target) configuration variable. The default limit is presently 100 entries. Raising the limit might allow more accurate planner estimates to be made, particularly for columns with irregular data distributions, at the price of consuming more space in `pg_statistic` and slightly more time to compute the estimates. Conversely, a lower limit might be sufficient for columns with simple data distributions.

## <a id="about_stats_collected"></a>About the Column Statistics Collected

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


## <a id="extended_stats"></a>Extended Statistics

It is common to see slow queries running bad execution plans because multiple columns used in the query clauses are correlated. The planner normally assumes that multiple conditions are independent of each other, an assumption that does not hold when column values are correlated. Regular statistics, because of their per-individual-column nature, cannot capture any knowledge about cross-column correlation. However, Greenplum Database has the ability to compute multivariate statistics, which can capture such information.

Because the number of possible column combinations is very large, it's impractical to compute multivariate statistics automatically. Instead, you can create extended statistics objects, more often called just statistics objects, to instruct the server to obtain statistics across interesting sets of columns.

You create statistics objects using the [CREATE STATISTICS](../../ref_guide/sql_commands/CREATE_STATISTICS.html) command. Creation of such an object merely creates a catalog entry expressing interest in the statistics. Actual data collection is performed by [ANALYZE](../../ref_guide/sql_commands/ANALYZE.html) \(either a manual command, or background auto-analyze\). The collected values can be examined in the [pg_statistic_ext_data](../../ref_guide/system_catalogs/pg_statistic_ext_data.html) catalog.

`ANALYZE` computes extended statistics based on the same sample of table rows that it takes for computing regular single-column statistics. Since the sample size is increased by increasing the statistics target for the table or any of its columns, a larger statistics target will normally result in more accurate extended statistics, as well as more time spent calculating them.

The following subsections describe the kinds of extended statistics that are currently supported.

### <a id="exstats_fd"></a>Functional Dependencies

The simplest kind of extended statistics tracks *functional dependencies*, a concept used in definitions of database normal forms. We say that column `b` is functionally dependent on column `a` if knowledge of the value of `a` is sufficient to determine the value of `b`, that is there are no two rows having the same value of `a` but different values of `b`. In a fully normalized database, functional dependencies should exist only on primary keys and superkeys. However, in practice many data sets are not fully normalized for various reasons; intentional denormalization for performance reasons is a common example. Even in a fully normalized database, there may be partial correlation between some columns, which can be expressed as partial functional dependency.

The existence of functional dependencies directly affects the accuracy of estimates in certain queries. If a query contains conditions on both the independent and the dependent column\(s\), the conditions on the dependent columns do not further reduce the result size; but without knowledge of the functional dependency, the query planner will assume that the conditions are independent, resulting in underestimating the result size.

To inform the planner about functional dependencies, `ANALYZE` can collect measurements of cross-column dependency. Assessing the degree of dependency between all sets of columns would be prohibitively expensive, so data collection is limited to those groups of columns appearing together in a statistics object defined with the dependencies option. It is advisable to create dependencies statistics only for column groups that are strongly correlated, to avoid unnecessary overhead in both `ANALYZE` and later query planning.

Here is an example of collecting functional-dependency statistics:

``` sql
CREATE STATISTICS stts (dependencies) ON city, zip FROM zipcodes;

ANALYZE zipcodes;

SELECT stxname, stxkeys, stxddependencies
  FROM pg_statistic_ext join pg_statistic_ext_data on (oid = stxoid)
  WHERE stxname = 'stts';
 stxname | stxkeys |             stxddependencies             
---------+---------+------------------------------------------
 stts    | 1 5     | {"1 => 5": 1.000000, "5 => 1": 0.423130}
(1 row)
```

Here it can be seen that column 1 \(zip code\) fully determines column 5 \(city\) so the coefficient is 1.0, while city only determines zip code about 42% of the time, meaning that there are many cities \(58%\) that are represented by more than a single ZIP code.

When computing the selectivity for a query involving functionally dependent columns, the planner adjusts the per-condition selectivity estimates using the dependency coefficients so as not to produce an underestimate.

#### <a id="exstats_fd_limit"></a>Limitations of Functional Dependencies

Functional dependencies are currently only applied when considering simple equality conditions that compare columns to constant values. They are not used to improve estimates for equality conditions comparing two columns or comparing a column to an expression, nor for range clauses, `LIKE` or any other type of condition.

When estimating with functional dependencies, the planner assumes that conditions on the involved columns are compatible and hence redundant. If they are incompatible, the correct estimate would be zero rows, but that possibility is not considered. For example, given a query like:

``` sql
SELECT * FROM zipcodes WHERE city = 'San Francisco' AND zip = '94105';
```

the planner will disregard the `city` clause as not changing the selectivity, which is correct. However, it will make the same assumption about:

``` sql
SELECT * FROM zipcodes WHERE city = 'San Francisco' AND zip = '90210';
```

even though there will really be zero rows satisfying this query. Functional dependency statistics do not provide enough information to conclude that, however.

In many practical situations, this assumption is usually satisfied; for example, there might be a GUI in the application that only allows selecting compatible city and ZIP code values to use in a query. But if that's not the case, functional dependencies may not be a viable option.

### <a id="exstats_nvndist"></a>Multivariate N-Distinct Counts

Single-column statistics store the number of distinct values in each column. Estimates of the number of distinct values when combining more than one column \(for example, for `GROUP BY a, b`\) are frequently wrong when the planner only has single-column statistical data, causing it to select bad plans.

To improve such estimates, `ANALYZE` can collect n-distinct statistics for groups of columns. As before, it's impractical to do this for every possible column grouping, so data is collected only for those groups of columns appearing together in a statistics object defined with the ndistinct option. Data will be collected for each possible combination of two or more columns from the set of listed columns.

Continuing the previous example, the n-distinct counts in a table of ZIP codes might look like the following:

``` sql
CREATE STATISTICS stts2 (ndistinct) ON city, state, zip FROM zipcodes;

ANALYZE zipcodes;

SELECT stxkeys AS k, stxdndistinct AS nd
  FROM pg_statistic_ext join pg_statistic_ext_data on (oid = stxoid)
  WHERE stxname = 'stts2';
-[ RECORD 1 ]--------------------------------------------------------
k  | 1 2 5
nd | {"1, 2": 33178, "1, 5": 33178, "2, 5": 27435, "1, 2, 5": 33178}
(1 row)
```

This indicates that there are three combinations of columns that have 33178 distinct values: ZIP code and state; ZIP code and city; and ZIP code, city and state \(the fact that they are all equal is expected given that ZIP code alone is unique in this table\). On the other hand, the combination of city and state has only 27435 distinct values.

It's advisable to create `ndistinct` statistics objects only on combinations of columns that are actually used for grouping, and for which misestimation of the number of groups is resulting in bad plans. Otherwise, the `ANALYZE` cycles are just wasted.

### <a id="exstats_mvmcv"></a>Multivariate Most-Common Value Lists

Another type of statistic stored for each column are most-common value (MCV) lists. This allows very accurate estimates for individual columns, but may result in significant misestimates for queries with conditions on multiple columns.

To improve such estimates, `ANALYZE` can collect MCV lists on combinations of columns. Similarly to functional dependencies and n-distinct coefficients, it's impractical to do this for every possible column grouping. Even more so in this case, as the MCV list \(unlike functional dependencies and n-distinct coefficients\) does store the common column values. So data is collected only for those groups of columns appearing together in a statistics object defined with the `mcv` option.

> **Note** The Greenplum Query Optimizer (GPORCA) does not support MCV list statistics.

Continuing the previous example, the MCV list for a table of ZIP codes might look like the following \(unlike for simpler types of statistics, a function is required for inspection of MCV contents\):

``` sql
CREATE STATISTICS stts3 (mcv) ON city, state FROM zipcodes;

ANALYZE zipcodes;

SELECT m.* FROM pg_statistic_ext join pg_statistic_ext_data on (oid = stxoid),
                pg_mcv_list_items(stxdmcv) m WHERE stxname = 'stts3';

 index |         values         | nulls | frequency | base_frequency 
-------+------------------------+-------+-----------+----------------
     0 | {Washington, DC}       | {f,f} |  0.003467 |        2.7e-05
     1 | {Apo, AE}              | {f,f} |  0.003067 |        1.9e-05
     2 | {Houston, TX}          | {f,f} |  0.002167 |       0.000133
     3 | {El Paso, TX}          | {f,f} |     0.002 |       0.000113
     4 | {New York, NY}         | {f,f} |  0.001967 |       0.000114
     5 | {Atlanta, GA}          | {f,f} |  0.001633 |        3.3e-05
     6 | {Sacramento, CA}       | {f,f} |  0.001433 |        7.8e-05
     7 | {Miami, FL}            | {f,f} |    0.0014 |          6e-05
     8 | {Dallas, TX}           | {f,f} |  0.001367 |        8.8e-05
     9 | {Chicago, IL}          | {f,f} |  0.001333 |        5.1e-05
   ...
(99 rows)
```

This indicates that the most common combination of city and state is Washington in DC, with actual frequency \(in the sample\) about 0.35%. The base frequency of the combination \(as computed from the simple per-column frequencies\) is only 0.0027%, resulting in two orders of magnitude under-estimates.

It's advisable to create MCV statistics objects only on combinations of columns that are actually used in conditions together, and for which misestimation of the number of groups is resulting in bad plans. Otherwise, the `ANALYZE` and planning cycles are just wasted.

#### <a id="es_mcv_func"></a>About Inspecting MCV Lists

Greenplum Database provides a function to inspect complex statistics defined using the [CREATE STATISTICS](../../ref_guide/sql_commands/CREATE_STATISTICS.html) command.

The `pg_mcv_list_items()` function returns a list of all items stored in a multi-column MCV list, and returns the following columns:

|Name|Type|Description|
|----|----|-----------|
| index | int | The index of the item in the MCV list. |
| values | text[] | The values stored in the MCV item. |
| nulls | boolean[] | Flags identifying NULL values. |
| frequency | double precision | The frequency of this MCV item. |
| base_frequency | double precision | The base frequency of this MCV item. |

You can use the `pg_mcv_list_items()` function as follows:

``` sql
SELECT m.* FROM pg_statistic_ext join pg_statistic_ext_data on (oid = stxoid),
    pg_mcv_list_items(stxdmcv) m WHERE stxname = 'stts';
```

Values of the `pg_mcv_list` can be obtained only from the [pg_statistic_ext_data](../../ref_guide/system_catalogs/pg_statistic_ext_data.html).`stxdmcv` column.

## <a id="multiex"></a>Multivariate Statistics Examples

### <a id="func_depend"></a>Functional Dependencies

Multivariate correlation can be demonstrated with a very simple data set — a table with two columns, both containing the same values:

``` sql
CREATE TABLE t (a INT, b INT);
INSERT INTO t SELECT i % 100, i % 100 FROM generate_series(1, 10000) s(i);
ANALYZE t;
```

As explained in Section 14.2, the Postgres Planner can determine cardinality of `t` using the number of pages and rows obtained from `pg_class`:

``` sql
SELECT relpages, reltuples FROM pg_class WHERE relname = 't';

 relpages | reltuples
----------+-----------
       45 |     10000
```

The data distribution is very simple; there are only 100 distinct values in each column, uniformly distributed.

The following example shows the result of estimating a `WHERE` condition on the a column:

``` sql
EXPLAIN (ANALYZE, TIMING OFF) SELECT * FROM t WHERE a = 1;
                                 QUERY PLAN                                  
-------------------------------------------------------------------------------
 Seq Scan on t  (cost=0.00..170.00 rows=100 width=8) (actual rows=100 loops=1)
   Filter: (a = 1)
   Rows Removed by Filter: 9900
```

The Postgres Planner examines the condition and determines the selectivity of this clause to be 1%. By comparing this estimate and the actual number of rows, we see that the estimate is very accurate \(in fact exact, as the table is very small\). Changing the `WHERE` condition to use the `b` column, an identical plan is generated. But observe what happens if we apply the same condition on both columns, combining them with `AND`:

``` sql
EXPLAIN (ANALYZE, TIMING OFF) SELECT * FROM t WHERE a = 1 AND b = 1;
                                 QUERY PLAN                                  
-----------------------------------------------------------------------------
 Seq Scan on t  (cost=0.00..195.00 rows=1 width=8) (actual rows=100 loops=1)
   Filter: ((a = 1) AND (b = 1))
   Rows Removed by Filter: 9900
```

The Postgres Planner estimates the selectivity for each condition individually, arriving at the same 1% estimates as above. Then it assumes that the conditions are independent, and so it multiplies their selectivities, producing a final selectivity estimate of just 0.01%. This is a significant underestimate, as the actual number of rows matching the conditions \(100\) is two orders of magnitude higher.

This problem can be fixed by creating a statistics object that directs `ANALYZE` to calculate functional-dependency multivariate statistics on the two columns:

``` sql
CREATE STATISTICS stts (dependencies) ON a, b FROM t;
ANALYZE t;
EXPLAIN (ANALYZE, TIMING OFF) SELECT * FROM t WHERE a = 1 AND b = 1;
                                  QUERY PLAN                                   
-------------------------------------------------------------------------------
 Seq Scan on t  (cost=0.00..195.00 rows=100 width=8) (actual rows=100 loops=1)
   Filter: ((a = 1) AND (b = 1))
   Rows Removed by Filter: 9900
```

### <a id="mvnd_counts"></a>Multivariate N-Distinct Counts

A similar problem occurs with estimation of the cardinality of sets of multiple columns, such as the number of groups that would be generated by a `GROUP BY` clause. When `GROUP BY `lists a single column, the n-distinct estimate \(which is visible as the estimated number of rows returned by the HashAggregate node\) is very accurate:

``` sql
EXPLAIN (ANALYZE, TIMING OFF) SELECT COUNT(*) FROM t GROUP BY a;
                                       QUERY PLAN                                        
-----------------------------------------------------------------------------------------
 HashAggregate  (cost=195.00..196.00 rows=100 width=12) (actual rows=100 loops=1)
   Group Key: a
   ->  Seq Scan on t  (cost=0.00..145.00 rows=10000 width=4) (actual rows=10000 loops=1)
```

But without multivariate statistics, the estimate for the number of groups in a query with two columns in `GROUP BY`, as in the following example, is off by an order of magnitude:

``` sql
EXPLAIN (ANALYZE, TIMING OFF) SELECT COUNT(*) FROM t GROUP BY a, b;
                                       QUERY PLAN                                        
--------------------------------------------------------------------------------------------
 HashAggregate  (cost=220.00..230.00 rows=1000 width=16) (actual rows=100 loops=1)
   Group Key: a, b
   ->  Seq Scan on t  (cost=0.00..145.00 rows=10000 width=8) (actual rows=10000 loops=1)
```

By redefining the statistics object to include n-distinct counts for the two columns, the estimate is much improved:

``` sql
DROP STATISTICS stts;
CREATE STATISTICS stts (dependencies, ndistinct) ON a, b FROM t;
ANALYZE t;
EXPLAIN (ANALYZE, TIMING OFF) SELECT COUNT(*) FROM t GROUP BY a, b;
                                       QUERY PLAN                                        
--------------------------------------------------------------------------------------------
 HashAggregate  (cost=220.00..221.00 rows=100 width=16) (actual rows=100 loops=1)
   Group Key: a, b
   ->  Seq Scan on t  (cost=0.00..145.00 rows=10000 width=8) (actual rows=10000 loops=1)
```

### <a id="mcv_lists"></a>MCV Lists

As explained in [Functional Dependencies](#func_depend) above, functional dependencies are very cheap and efficient type of statistics, but their main limitation is their global nature \(only tracking dependencies at the column level, not between individual column values\).

This section introduces multivariate variant of MCV \(most-common values\) lists, a straightforward extension of the per-column statistics described in Section 71.1. These statistics address the limitation by storing individual values, but it is naturally more expensive, both in terms of building the statistics in `ANALYZE`, storage and planning time.

Let's look at the query from the *Functional Dependencies* section again, but this time with a MCV list created on the same set of columns \(be sure to drop the functional dependencies, to make sure the Postgres Planner uses the newly created statistics\).

``` sql
DROP STATISTICS stts;
CREATE STATISTICS stts2 (mcv) ON a, b FROM t;
ANALYZE t;
EXPLAIN (ANALYZE, TIMING OFF) SELECT * FROM t WHERE a = 1 AND b = 1;
                                   QUERY PLAN
-------------------------------------------------------------------------------
 Seq Scan on t  (cost=0.00..195.00 rows=100 width=8) (actual rows=100 loops=1)
   Filter: ((a = 1) AND (b = 1))
   Rows Removed by Filter: 9900
```

The estimate is as accurate as with the functional dependencies, mostly thanks to the table being fairly small and having a simple distribution with a low number of distinct values. Before looking at the second query, which was not handled by functional dependencies particularly well, let's inspect the MCV list a bit.

Inspecting the MCV list is possible using the `pg_mcv_list_items()` set-returning function:

``` sql
SELECT m.* FROM pg_statistic_ext join pg_statistic_ext_data on (oid = stxoid),
                pg_mcv_list_items(stxdmcv) m WHERE stxname = 'stts2';
 index |  values  | nulls | frequency | base_frequency 
-------+----------+-------+-----------+----------------
     0 | {0, 0}   | {f,f} |      0.01 |         0.0001
     1 | {1, 1}   | {f,f} |      0.01 |         0.0001
   ...
    49 | {49, 49} | {f,f} |      0.01 |         0.0001
    50 | {50, 50} | {f,f} |      0.01 |         0.0001
   ...
    97 | {97, 97} | {f,f} |      0.01 |         0.0001
    98 | {98, 98} | {f,f} |      0.01 |         0.0001
    99 | {99, 99} | {f,f} |      0.01 |         0.0001
(100 rows)
```

This confirms that there are 100 distinct combinations in the two columns, and all of them are about equally likely \(1% frequency for each one\). The base frequency is the frequency computed from per-column statistics, as if there were no multi-column statistics. Had there been any null values in either of the columns, this would be identified in the nulls column.

When estimating the selectivity, the Postgres Planner applies all the conditions on items in the MCV list, and then sums the frequencies of the matching ones.

Compared to functional dependencies, MCV lists have two major advantages. Firstly, the list stores actual values, making it possible to decide which combinations are compatible.

``` sql
EXPLAIN (ANALYZE, TIMING OFF) SELECT * FROM t WHERE a = 1 AND b = 10;
                                 QUERY PLAN
---------------------------------------------------------------------------
 Seq Scan on t  (cost=0.00..195.00 rows=1 width=8) (actual rows=0 loops=1)
   Filter: ((a = 1) AND (b = 10))
   Rows Removed by Filter: 10000
```

Secondly, MCV lists handle a wider range of clause types, not just equality clauses like functional dependencies. For example, consider the following range query for the same table:

``` sql
EXPLAIN (ANALYZE, TIMING OFF) SELECT * FROM t WHERE a <= 49 AND b > 49;
                                QUERY PLAN
---------------------------------------------------------------------------
 Seq Scan on t  (cost=0.00..195.00 rows=1 width=8) (actual rows=0 loops=1)
   Filter: ((a <= 49) AND (b > 49))
   Rows Removed by Filter: 10000
```
