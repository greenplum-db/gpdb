# Schema Design 

Best practices for designing Greenplum Database schemas.

Greenplum Database is an analytical, shared-nothing database, which is much different than a highly normalized, transactional SMP database. Greenplum Database performs best with a denormalized schema design suited for MPP analytical processing, a star or snowflake schema, with large centralized fact tables connected to multiple smaller dimension tables.

**Parent topic:** [Greenplum Database Best Practices](intro.html)

## Data Types 

### Use Types Consistently 

Use the same data types for columns used in joins between tables. If the data types differ, Greenplum Database must dynamically convert the data type of one of the columns so the data values can be compared correctly. With this in mind, you may need to increase the data type size to facilitate joins to other common objects.

### Choose Data Types that Use the Least Space 

You can increase database capacity and improve query execution by choosing the most efficient data types to store your data.

Use `TEXT` or `VARCHAR` rather than `CHAR`. There are no performance differences among the character data types, but using `TEXT` or `VARCHAR` can decrease the storage space used.

Use the smallest numeric data type that will accommodate your data. Using `BIGINT` for data that fits in `INT` or `SMALLINT` wastes storage space.

## Storage Model 

Greenplum Database provides an array of storage options when creating tables. It is very important to know when to use heap storage versus append-optimized \(AO\) storage, and when to use row-oriented storage versus column-oriented storage. The correct selection of heap versus AO and row versus column is extremely important for large fact tables, but less important for small dimension tables.

The best practices for determining the storage model are:

1.  Design and build an insert-only model, truncating a daily partition before load.
2.  For large partitioned fact tables, evaluate and use optimal storage options for different partitions. One storage option is not always right for the entire partitioned table. For example, some partitions can be row-oriented while others are column-oriented.
3.  When using column-oriented storage, every column is a separate file on *every* Greenplum Database segment. For tables with a large number of columns consider columnar storage for data often accessed \(hot\) and row-oriented storage for data not often accessed \(cold\).
4.  Storage options should be set at the partition level.
5.  Compress large tables to improve I/O performance and to make space in the cluster.

### Heap Storage or Append-Optimized Storage 

Heap storage is the default model, and is the model PostgreSQL uses for all database tables. Use heap storage for tables and partitions that will receive iterative `UPDATE`, `DELETE`, and singleton `INSERT` operations. Use heap storage for tables and partitions that will receive concurrent `UPDATE`, `DELETE`, and `INSERT` operations.

Use append-optimized storage for tables and partitions that are updated infrequently after the initial load and have subsequent inserts performed only in batch operations. Avoid performing singleton `INSERT`, `UPDATE`, or `DELETE` operations on append-optimized tables. Concurrent batch `INSERT` operations are acceptable, but *never* perform concurrent batch `UPDATE` or `DELETE` operations.

The append-optimized storage model is inappropriate for frequently updated tables, because space occupied by rows that are updated and deleted in append-optimized tables is not recovered and reused as efficiently as with heap tables. Append-optimized storage is intended for large tables that are loaded once, updated infrequently, and queried frequently for analytical query processing.

### Row or Column Orientation 

Row orientation is the traditional way to store database tuples. The columns that comprise a row are stored on disk contiguously, so that an entire row can be read from disk in a single I/O.

Column orientation stores column values together on disk. A separate file is created for each column. If the table is partitioned, a separate file is created for each column and partition. When a query accesses only a small number of columns in a column-oriented table with many columns, the cost of I/O is substantially reduced compared to a row-oriented table; any columns not referenced do not have to be retrieved from disk.

Row-oriented storage is recommended for transactional type workloads with iterative transactions where updates are required and frequent inserts are performed. Use row-oriented storage when selects against the table are wide, where many columns of a single row are needed in a query. If the majority of columns in the `SELECT` list or `WHERE` clause is selected in queries, use row-oriented storage. Use row-oriented storage for general purpose or mixed workloads, as it offers the best combination of flexibility and performance.

Column-oriented storage is optimized for read operations but it is not optimized for write operations; column values for a row must be written to different places on disk. Column-oriented tables can offer optimal query performance on large tables with many columns where only a small subset of columns are accessed by the queries.

Another benefit of column orientation is that a collection of values of the same data type can be stored together in less space than a collection of mixed type values, so column-oriented tables use less disk space \(and consequently less disk I/O\) than row-oriented tables. Column-oriented tables also compress better than row-oriented tables.

Use column-oriented storage for data warehouse analytic workloads where selects are narrow or aggregations of data are computed over a small number of columns. Use column-oriented storage for tables that have single columns that are regularly updated without modifying other columns in the row. Reading a complete row in a wide columnar table requires more time than reading the same row from a row-oriented table. It is important to understand that each column is a separate physical file on *every* segment in Greenplum Database.

## Compression 

Greenplum Database offers a variety of options to compress append-optimized tables and partitions. Use compression to improve I/O across the system by allowing more data to be read with each disk read operation. The best practice is to set the column compression settings at the partition level.

Note that new partitions added to a partitioned table do not automatically inherit compression defined at the table level; you must *specifically* define compression when you add new partitions.

Run-length encoding \(RLE\) compression provides the best levels of compression. Higher levels of compression usually result in more compact storage on disk, but require additional time and CPU cycles when compressing data on writes and uncompressing on reads. Sorting data, in combination with the various compression options, can achieve the highest level of compression.

Data compression should never be used for data that is stored on a compressed file system.

Test different compression types and ordering methods to determine the best compression for your specific data. For example, you might start zlib compression at level 4 or 5 and adjust for best results. RLE compression works best with files that contain repetitive data.

## Distributions 

An optimal distribution that results in evenly distributed data is the most important factor in Greenplum Database. In an MPP shared nothing environment overall response time for a query is measured by the completion time for all segments. The system is only as fast as the slowest segment. If the data is skewed, segments with more data will take more time to complete, so every segment must have an approximately equal number of rows and perform approximately the same amount of processing. Poor performance and out of memory conditions may result if one segment has significantly more data to process than other segments.

Consider the following best practices when deciding on a distribution strategy:

-   Explicitly define a column or random distribution for all tables. Do not use the default.
-   Ideally, use a single column that will distribute data across all segments evenly.
-   Do not distribute on columns that will be used in the `WHERE` clause of a query.
-   Do not distribute on dates or timestamps.
-   The distribution key column data should contain unique values or very high cardinality.
-   If a single column cannot achieve an even distribution, use a multi-column distribution key with a maximum of two columns. Additional column values do not typically yield a more even distribution and they require additional time in the hashing process.
-   If a two-column distribution key cannot achieve an even distribution of data, use a random distribution. Multi-column distribution keys in most cases require motion operations to join tables, so they offer no advantages over a random distribution.

Greenplum Database random distribution is not round-robin, so there is no guarantee of an equal number of records on each segment. Random distributions typically fall within a target range of less than ten percent variation.

Optimal distributions are critical when joining large tables together. To perform a join, matching rows must be located together on the same segment. If data is not distributed on the same join column, the rows needed from one of the tables are dynamically redistributed to the other segments. In some cases a broadcast motion, in which each segment sends its individual rows to all other segments, is performed rather than a redistribution motion, where each segment rehashes the data and sends the rows to the appropriate segments according to the hash key.

### Local \(Co-located\) Joins 

Using a hash distribution that evenly distributes table rows across all segments and results in local joins can provide substantial performance gains. When joined rows are on the same segment, much of the processing can be accomplished within the segment instance. These are called *local* or *co-located* joins. Local joins minimize data movement; each segment operates independently of the other segments, without network traffic or communications between segments.

To achieve local joins for large tables commonly joined together, distribute the tables on the same column. Local joins require that both sides of a join be distributed on the same columns \(and in the same order\) *and* that all columns in the distribution clause are used when joining tables. The distribution columns must also be the same data type—although some values with different data types may appear to have the same representation, they are stored differently and hash to different values, so they are stored on different segments.

### Data Skew 

Data skew is often the root cause of poor query performance and out of memory conditions. Skewed data affects scan \(read\) performance, but it also affects all other query execution operations, for instance, joins and group by operations.

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

### Processing Skew 

Processing skew results when a disproportionate amount of data flows to, and is processed by, one or a few segments. It is often the culprit behind Greenplum Database performance and stability issues. It can happen with operations such join, sort, aggregation, and various OLAP operations. Processing skew happens in flight while a query is executing and is not as easy to detect as data skew, which is caused by uneven data distribution due to the wrong choice of distribution keys. Data skew is present at the table level, so it can be easily detected and avoided by choosing optimal distribution keys.

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

## Partitioning 

A good partitioning strategy reduces the amount of data to be scanned by reading only the partitions needed to satisfy a query.

Each partition is a separate physical file or set of tiles \(in the case of column-oriented tables\) on *every* segment. Just as reading a complete row in a wide columnar table requires more time than reading the same row from a heap table, *reading all partitions in a partitioned table requires more time than reading the same data from a non-partitioned table*.

Following are partitioning best practices:

-   Partition large tables only, do not partition small tables.
-   Use partitioning on large tables *only* when partition elimination \(partition pruning\) can be achieved based on query criteria and is accomplished by partitioning the table based on the query predicate. Whenever possible, use range partitioning instead of list partitioning.
-   The query planner can selectively scan partitioned tables only when the query contains a direct and simple restriction of the table using immutable operators, such as `=`, `<` , `<=`, `>`, `>=`, and `<>`.
-   Selective scanning recognizes `STABLE` and `IMMUTABLE` functions, but does not recognize `VOLATILE` functions within a query. For example, `WHERE` clauses such as

    ```
    date > CURRENT_DATE
    ```

    cause the query planner to selectively scan partitioned tables, but a `WHERE` clause such as

    ```
    time > TIMEOFDAY
    ```

    does not. It is important to validate that queries are selectively scanning partitioned tables \(partitions are being eliminated\) by examining the query `EXPLAIN` plan.

-   Do not use default partitions. The default partition is always scanned but, more importantly, in many environments they tend to overfill resulting in poor performance.
-   *Never* partition and distribute tables on the same column.
-   Do not use multi-level partitioning. While sub-partitioning is supported, it is not recommended because typically subpartitions contain little or no data. It is a myth that performance increases as the number of partitions or subpartitions increases; the administrative overhead of maintaining many partitions and subpartitions will outweigh any performance benefits. For performance, scalability and manageability, balance partition scan performance with the number of overall partitions.
-   Beware of using too many partitions with column-oriented storage.
-   Consider workload concurrency and the average number of partitions opened and scanned for all concurrent queries.

### Number of Partition and Columnar Storage Files 

The only hard limit for the number of files Greenplum Database supports is the operating system's open file limit. It is important, however, to consider the total number of files in the cluster, the number of files on every segment, and the total number of files on a host. In an MPP shared nothing environment, every node operates independently of other nodes. Each node is constrained by its disk, CPU, and memory. CPU and I/O constraints are not common with Greenplum Database, but memory is often a limiting factor because the query execution model optimizes query performance in memory.

The optimal number of files per segment also varies based on the number of segments on the node, the size of the cluster, SQL access, concurrency, workload, and skew. There are generally six to eight segments per host, but large clusters should have fewer segments per host. When using partitioning and columnar storage it is important to balance the total number of files in the cluster ,but it is *more* important to consider the number of files per segment and the total number of files on a node.

Example DCA 64GB Memory per Node

-   Number of nodes: 16
-   Number of segments per node: 8
-   Average number of files per segment: 10,000

The total number of files per node is `8*10,000 = 80,000` and the total number of files for the cluster is `8*16*10,000 = 1,280,000`. The number of files increases quickly as the number of partitions and the number of columns increase.

As a general best practice, limit the total number of files per node to under 100,000. As the previous example shows, the optimal number of files per segment and total number of files per node depends on the hardware configuration for the nodes \(primarily memory\), size of the cluster, SQL access, concurrency, workload and skew.

## Indexes 

Indexes are not generally needed in Greenplum Database. Most analytical queries operate on large volumes of data, while indexes are intended for locating single rows or small numbers of rows of data. In Greenplum Database, a sequential scan is an efficient method to read data as each segment contains an equal portion of the data and all segments work in parallel to read the data.

If adding an index does not produce performance gains, drop it. Verify that every index you create is used by the optimizer.

For queries with high selectivity, indexes may improve query performance. Create an index on a single column of a columnar table for drill through purposes for high cardinality columns that are required for highly selective queries.

Do not index columns that are frequently updated. Creating an index on a column that is frequently updated increases the number of writes required on updates.

Indexes on expressions should be used only if the expression is used frequently in queries.

An index with a predicate creates a partial index that can be used to select a small number of rows from large tables.

Avoid overlapping indexes. Indexes that have the same leading column are redundant.

Indexes can improve performance on compressed append-optimized tables for queries that return a targeted set of rows. For compressed data, an index access method means only the necessary pages are uncompressed.

Create selective B-tree indexes. Index selectivity is a ratio of the number of distinct values a column has divided by the number of rows in a table. For example, if a table has 1000 rows and a column has 800 distinct values, the selectivity of the index is 0.8, which is considered good.

As a general rule, drop indexes before loading data into a table. The load will run an order of magnitude faster than loading data into a table with indexes. After the load, re-create the indexes.

Bitmap indexes are suited for querying and not updating. Bitmap indexes perform best when the column has a low cardinality—100 to 100,000 distinct values. Do not use bitmap indexes for unique columns, very high, or very low cardinality data. Do not use bitmap indexes for transactional workloads.

If indexes are needed on partitioned tables, the index columns must be different than the partition columns. A benefit of indexing partitioned tables is that because the b-tree performance degrades exponentially as the size of the b-tree grows, creating indexes on partitioned tables creates smaller b-trees that perform better than with non-partitioned tables.

## Column Sequence and Byte Alignment 

For optimum performance lay out the columns of a table to achieve data type byte alignment. Lay out the columns in heap tables in the following order:

1.  Distribution and partition columns
2.  Fixed numeric types
3.  Variable data types

Lay out the data types from largest to smallest, so that `BIGINT` and `TIMESTAMP` come before `INT` and `DATE`, and all of these types come before `TEXT`, `VARCHAR`, or `NUMERIC(x,y)`. For example, 8-byte types first \(`BIGINT`, `TIMESTAMP`\), 4-byte types next \(`INT`, `DATE`\), 2-byte types next \(`SMALLINT`\), and variable data type last \(`VARCHAR`\).

Instead of defining columns in this sequence:

`Int`, `Bigint`, `Timestamp`, `Bigint`, `Timestamp`, `Int` \(distribution key\), `Date` \(partition key\), `Bigint`, `Smallint`

define the columns in this sequence:

`Int` \(distribution key\), `Date` \(partition key\), `Bigint`, `Bigint`, `Timestamp`, `Bigint`, `Timestamp`, `Int`, `Smallint`

