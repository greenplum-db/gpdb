# Distribution and Skew 

Greenplum Database relies on even distribution of data across segments.

In an MPP shared nothing environment, overall response time for a query is measured by the completion time for all segments. The system is only as fast as the slowest segment. If the data is skewed, segments with more data will take more time to complete, so every segment must have an approximately equal number of rows and perform approximately the same amount of processing. Poor performance and out of memory conditions may result if one segment has significantly more data to process than other segments.

Optimal distributions are critical when joining large tables together. To perform a join, matching rows must be located together on the same segment. If data is not distributed on the same join column, the rows needed from one of the tables are dynamically redistributed to the other segments. In some cases a broadcast motion, in which each segment sends its individual rows to all other segments, is performed rather than a redistribution motion, where each segment rehashes the data and sends the rows to the appropriate segments according to the hash key.

**Parent topic:** [Greenplum Database Administrator Guide](admin_guide.html)

## Local \(Co-located\) Joins 

Using a hash distribution that evenly distributes table rows across all segments and results in local joins can provide substantial performance gains. When joined rows are on the same segment, much of the processing can be accomplished within the segment instance. These are called *local* or *co-located* joins. Local joins minimize data movement; each segment operates independently of the other segments, without network traffic or communications between segments.

To achieve local joins for large tables commonly joined together, distribute the tables on the same column. Local joins require that both sides of a join be distributed on the same columns \(and in the same order\) *and* that all columns in the distribution clause are used when joining tables. The distribution columns must also be the same data type—although some values with different data types may appear to have the same representation, they are stored differently and hash to different values, so they are stored on different segments.

## Data Skew 

Data skew may be caused by uneven data distribution due to the wrong choice of distribution keys or single tuple table insert or copy operations. Present at the table level, data skew, is often the root cause of poor query performance and out of memory conditions. Skewed data affects scan \(read\) performance, but it also affects all other query execution operations, for instance, joins and group by operations.

It is very important to *validate* distributions to *ensure* that data is evenly distributed after the initial load. It is equally important to *continue* to validate distributions after incremental loads.

The following query shows the number of rows per segment as well as the variance from the minimum and maximum numbers of rows:

```
SELECT 'Example Table' AS "Table Name", 
    max(c) AS "Max Seg Rows", min(c) AS "Min Seg Rows", 
    (max(c)-min(c))*100.0/max(c) AS "Percentage Difference Between Max & Min" 
FROM (SELECT count(*) c, gp_segment_id FROM facts GROUP BY 2) AS a;
```

The `gp_toolkit` schema has two views that you can use to check for skew.

-   The `gp_toolkit.gp_skew_coefficients` view shows data distribution skew by calculating the coefficient of variation \(CV\) for the data stored on each segment. The `skccoeff` column shows the coefficient of variation \(CV\), which is calculated as the standard deviation divided by the average. It takes into account both the average and variability around the average of a data series. The lower the value, the better. Higher values indicate greater data skew.
-   The `gp_toolkit.gp_skew_idle_fractions` view shows data distribution skew by calculating the percentage of the system that is idle during a table scan, which is an indicator of computational skew. The `siffraction` column shows the percentage of the system that is idle during a table scan. This is an indicator of uneven data distribution or query processing skew. For example, a value of 0.1 indicates 10% skew, a value of 0.5 indicates 50% skew, and so on. Tables that have more than10% skew should have their distribution policies evaluated.

## Processing Skew 

Processing skew results when a disproportionate amount of data flows to, and is processed by, one or a few segments. It is often the culprit behind Greenplum Database performance and stability issues. It can happen with operations such join, sort, aggregation, and various OLAP operations. Processing skew happens in flight while a query is executing and is not as easy to detect as data skew.

If single segments are failing, that is, not all segments on a host, it may be a processing skew issue. Identifying processing skew is currently a manual process. First look for spill files. If there is skew, but not enough to cause spill, it will not become a performance issue. If you determine skew exists, then find the query responsible for the skew. Following are the steps and commands to use. \(Change names like the host file name passed to `gpssh` accordingly\):

1.  Find the OID for the database that is to be monitored for skew processing:

    ```
    SELECT oid, datname FROM pg_database;
    ```

    Example output:

    ```
      oid  |  datname
    -------+-----------
     17088 | gpadmin
     10899 | postgres
         1 | template1
     10898 | template0
     38817 | pws
     39682 | gpperfmon
    (6 rows)
    
    ```

2.  Run a `gpssh` command to check file sizes across all of the segment nodes in the system. Replace `<OID>` with the OID of the database from the prior command:

    ```
    [gpadmin@mdw kend]$ gpssh -f ~/hosts -e \
        "du -b /data[1-2]/primary/gpseg*/base/<<OID>>/pgsql_tmp/*" | \
        grep -v "du -b" | sort | awk -F" " '{ arr[$1] = arr[$1] + $2 ; tot = tot + $2 }; END \
        { for ( i in arr ) print "Segment node" i, arr[i], "bytes (" arr[i]/(1024**3)" GB)"; \
        print "Total", tot, "bytes (" tot/(1024**3)" GB)" }' -
    ```

    Example output:

    ```
    Segment node[sdw1] 2443370457 bytes (2.27557 GB)
    Segment node[sdw2] 1766575328 bytes (1.64525 GB)
    Segment node[sdw3] 1761686551 bytes (1.6407 GB)
    Segment node[sdw4] 1780301617 bytes (1.65804 GB)
    Segment node[sdw5] 1742543599 bytes (1.62287 GB)
    Segment node[sdw6] 1830073754 bytes (1.70439 GB)
    Segment node[sdw7] 1767310099 bytes (1.64594 GB)
    Segment node[sdw8] 1765105802 bytes (1.64388 GB)
    Total 14856967207 bytes (13.8366 GB)
    ```

    If there is a *significant and sustained* difference in disk usage, then the queries being executed should be investigated for possible skew \(the example output above does not reveal significant skew\). In monitoring systems, there will always be some skew, but often it is *transient* and will be *short in duration*.

3.  If significant and sustained skew appears, the next task is to identify the offending query.

    The command in the previous step sums up the entire node. This time, find the actual segment directory. You can do this from the master or by logging into the specific node identified in the previous step. Following is an example run from the master.

    This example looks specifically for sort files. Not all spill files or skew situations are caused by sort files, so you will need to customize the command:

    ```
    $ gpssh -f ~/hosts -e 
        "ls -l /data[1-2]/primary/gpseg*/base/19979/pgsql_tmp/*" 
        | grep -i sort | awk '{sub(/base.*tmp\//, ".../", $10); print $1,$6,$10}' | sort -k2 -n
    ```

    Here is output from this command:

    ```
    [sdw1] 288718848
          /data1/primary/gpseg2/.../pgsql_tmp_slice0_sort_17758_0001.0[sdw1] 291176448
          /data2/primary/gpseg5/.../pgsql_tmp_slice0_sort_17764_0001.0[sdw8] 924581888
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0010.9[sdw4] 980582400
          /data1/primary/gpseg18/.../pgsql_tmp_slice10_sort_29425_0001.0[sdw6] 986447872
          /data2/primary/gpseg35/.../pgsql_tmp_slice10_sort_29602_0001.0...[sdw5] 999620608
          /data1/primary/gpseg26/.../pgsql_tmp_slice10_sort_28637_0001.0[sdw2] 999751680
          /data2/primary/gpseg9/.../pgsql_tmp_slice10_sort_3969_0001.0[sdw3] 1000112128
          /data1/primary/gpseg13/.../pgsql_tmp_slice10_sort_24723_0001.0[sdw5] 1000898560
          /data2/primary/gpseg28/.../pgsql_tmp_slice10_sort_28641_0001.0...[sdw8] 1008009216
          /data1/primary/gpseg44/.../pgsql_tmp_slice10_sort_15671_0001.0[sdw5] 1008566272
          /data1/primary/gpseg24/.../pgsql_tmp_slice10_sort_28633_0001.0[sdw4] 1009451008
          /data1/primary/gpseg19/.../pgsql_tmp_slice10_sort_29427_0001.0[sdw7] 1011187712
          /data1/primary/gpseg37/.../pgsql_tmp_slice10_sort_18526_0001.0[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0001.0[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0002.1[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0003.2[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0004.3[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0005.4[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0006.5[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0007.6[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0008.7[sdw8] 1573741824
          /data2/primary/gpseg45/.../pgsql_tmp_slice10_sort_15673_0009.8
    ```

    Scanning this output reveals that segment `gpseg45` on host `sdw8` is the culprit, as its sort files are larger than the others in the output.

4.  Log in to the offending node with `ssh` and become root. Use the `lsof` command to find the PID for the process that owns one of the sort files:

    ```
    [root@sdw8 ~]# lsof /data2/primary/gpseg45/base/19979/pgsql_tmp/pgsql_tmp_slice10_sort_15673_0002.1
    COMMAND  PID    USER    FD   TYPE DEVICE  SIZE        NODE        NAME
    postgres 15673  gpadmin 11u  REG  8,48    1073741824  64424546751 /data2/primary/gpseg45/base/19979/pgsql_tmp/pgsql_tmp_slice10_sort_15673_0002.1
    ```

    The PID, `15673`, is also part of the file name, but this may not always be the case.

5.  Use the `ps` command with the PID to identify the database and connection information:

    ```
    [root@sdw8 ~]# ps -eaf | grep 15673
    gpadmin  15673 27471 28 12:05 ?        00:12:59 postgres: port 40003, sbaskin bdw
            172.28.12.250(21813) con699238 seg45 cmd32 slice10 MPPEXEC SELECT
    root     29622 29566  0 12:50 pts/16   00:00:00 grep 15673
    ```

6.  On the master, check the `pg_log` log file for the user in the previous command \(`sbaskin`\), connection \(`con699238`, and command \(`cmd32`\). The line in the log file with these three values *should* be the line that contains the query, but occasionally, the command number may differ slightly. For example, the `ps` output may show `cmd32`, but in the log file it is `cmd34`. If the query is still running, the last query for the user and connection is the offending query.

The remedy for processing skew in almost all cases is to rewrite the query. Creating temporary tables can eliminate skew. Temporary tables can be randomly distributed to force a two-stage aggregation.

