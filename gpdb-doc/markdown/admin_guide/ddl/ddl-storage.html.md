---
title: Choosing the Table Storage Model 
---

Greenplum Database supports several storage models and a mix of storage models. When you create a table, you choose how to store its data. This topic explains the options for table storage and how to choose the best storage model for your workload.

-   [Heap Storage](#topic37)
-   [Append-Optimized Storage](#topic38)
-   [Choosing Row or Column-Oriented Storage](#topic39)
-   [Using Compression \(Append-Optimized Tables Only\)](#topic40)
-   [Checking the Compression and Distribution of an Append-Optimized Table](#topic41)
-   [Altering a Table](#topic55)
-   [Dropping a Table](#topic62)

> **Note** To simplify the creation of database tables, you can specify the default values for some table storage options with the Greenplum Database server configuration parameter `gp_default_storage_options`.

For information about the parameter, see "Server Configuration Parameters" in the *Greenplum Database Reference Guide*.

**Parent topic:** [Defining Database Objects](../ddl/ddl.html)

## <a id="topic37"></a>Heap Storage 

By default, Greenplum Database uses the same heap storage model as PostgreSQL. Heap table storage works best with OLTP-type workloads where the data is often modified after it is initially loaded. `UPDATE` and `DELETE` operations require storing row-level versioning information to ensure reliable database transaction processing. Heap tables are best suited for smaller tables, such as dimension tables, that are often updated after they are initially loaded.

## <a id="topic38"></a>Append-Optimized Storage 

Append-optimized table storage works best with denormalized fact tables in a data warehouse environment. Denormalized fact tables are typically the largest tables in the system. Fact tables are usually loaded in batches and accessed by read-only queries. Moving large fact tables to an append-optimized storage model eliminates the storage overhead of the per-row update visibility information, saving about 20 bytes per row. This allows for a leaner and easier-to-optimize page structure. The storage model of append-optimized tables is optimized for bulk data loading. Single row `INSERT` statements are not recommended.

### <a id="im168504"></a>To create a heap table 

Row-oriented heap tables are the default storage type.

```
=> CREATE TABLE foo (a int, b text) DISTRIBUTED BY (a);
```

Use the `WITH` clause of the `CREATE TABLE` command to declare the table storage options. The default is to create the table as a regular row-oriented heap-storage table. For example, to create an append-optimized table with no compression:

```
=> CREATE TABLE bar (a int, b text) 
    WITH (appendoptimized=true)
    DISTRIBUTED BY (a);
```

> **Note** You use the `appendoptimized=value` syntax to specify the append-optimized table storage type. `appendoptimized` is a thin alias for the `appendonly` legacy storage option. Greenplum Database stores `appendonly` in the catalog, and displays the same when listing storage options for append-optimized tables.

`UPDATE` and `DELETE` are not allowed on append-optimized tables in a repeatable read or serizalizable transaction and will cause the transaction to end prematurely. `DECLARE...FOR UPDATE` and triggers are not supported with append-optimized tables.  `CLUSTER` on append-optimized tables is only supported over B-tree indexes.

## <a id="topic39"></a>Choosing Row or Column-Oriented Storage 

Greenplum provides a choice of storage orientation models: row, column, or a combination of both. This topic provides general guidelines for choosing the optimum storage orientation for a table. Evaluate performance using your own data and query workloads.

-   Row-oriented storage: good for OLTP types of workloads with many iterative transactions and many columns of a single row needed all at once, so retrieving is efficient.
-   Column-oriented storage: good for data warehouse workloads with aggregations of data computed over a small number of columns, or for single columns that require regular updates without modifying other column data.

For most general purpose or mixed workloads, row-oriented storage offers the best combination of flexibility and performance. However, there are use cases where a column-oriented storage model provides more efficient I/O and storage. Consider the following requirements when deciding on the storage orientation model for a table:

-   **Updates of table data.** If you load and update the table data frequently, choose a row-orientedheap table. Column-oriented table storage is only available on append-optimized tables.

    See [Heap Storage](#topic37) for more information.

-   **Frequent INSERTs.** If rows are frequently inserted into the table, consider a row-oriented model. Column-oriented tables are not optimized for write operations, as column values for a row must be written to different places on disk.
-   **Number of columns requested in queries.** If you typically request all or the majority of columns in the `SELECT` list or `WHERE` clause of your queries, consider a row-oriented model. Column-oriented tables are best suited to queries that aggregate many values of a single column where the `WHERE` or `HAVING` predicate is also on the aggregate column. For example:

    ```
    SELECT SUM(salary)...
    ```

    ```
    SELECT AVG(salary)... WHERE salary > 10000
    ```

    Or where the `WHERE` predicate is on a single column and returns a relatively small number of rows. For example:

    ```
    SELECT salary, dept ... WHERE state='CA'
    ```

-   **Number of columns in the table.** Row-oriented storage is more efficient when many columns are required at the same time, or when the row-size of a table is relatively small. Column-oriented tables can offer better query performance on tables with many columns where you access a small subset of columns in your queries.
-   **Compression.** Column data has the same data type, so storage size optimizations are available in column-oriented data that are not available in row-oriented data. For example, many compression schemes use the similarity of adjacent data to compress. However, the greater adjacent compression achieved, the more difficult random access can become, as data must be uncompressed to be read.

### <a id="im169305"></a>To create a column-oriented table 

The `WITH` clause of the `CREATE TABLE` command specifies the table's storage options. The default is a row-orientedheap table. Tables that use column-oriented storage must be append-optimized tables. For example, to create a column-oriented table:

```
=> CREATE TABLE bar (a int, b text) 
    WITH (appendoptimized=true, orientation=column)
    DISTRIBUTED BY (a);

```

## <a id="topic40"></a>Using Compression \(Append-Optimized Tables Only\) 

There are two types of in-database compression available in the Greenplum Database for append-optimized tables:

-   Table-level compression is applied to an entire table.
-   Column-level compression is applied to a specific column. You can apply different column-level compression algorithms to different columns.

The following table summarizes the available compression algorithms.

|Table Orientation|Available Compression Types|Supported Algorithms|
|-----------------|---------------------------|--------------------|
|Row|Table|`ZLIB`, `ZSTD`, and `QUICKLZ`\*|
|Column|Column and Table|`RLE_TYPE`, `ZLIB`, `ZSTD`, and `QUICKLZ`\*|

> **Note** \*QuickLZ compression is not available in the open source version of Greenplum Database.

When choosing a compression type and level for append-optimized tables, consider these factors:

-   CPU usage. Your segment systems must have the available CPU power to compress and uncompress the data.
-   Compression ratio/disk size. Minimizing disk size is one factor, but also consider the time and CPU capacity required to compress and scan data. Find the optimal settings for efficiently compressing data without causing excessively long compression times or slow scan rates.
-   Speed of compression. QuickLZ compression generally uses less CPU capacity and compresses data faster at a lower compression ratio than zlib. zlib provides higher compression ratios at lower speeds.

    For example, at compression level 1 \(`compresslevel=1`\), QuickLZ and zlib have comparable compression ratios, though at different speeds. Using zlib with `compresslevel=6` can significantly increase the compression ratio compared to QuickLZ, though with lower compression speed. Zstandard compression can provide for either good compression ratio or speed, depending on compression level, or a good compromise on both.

-   Speed of decompression/scan rate. Performance with compressed append-optimized tables depends on hardware, query tuning settings, and other factors. Perform comparison testing to determine the actual performance in your environment.

    > **Note** Do not create compressed append-optimized tables on file systems that use compression. If the file system on which your segment data directory resides is a compressed file system, your append-optimized table must not use compression.


Performance with compressed append-optimized tables depends on hardware, query tuning settings, and other factors. You should perform comparison testing to determine the actual performance in your environment.

> **Note** Zstd compression level can be set to values between 1 and 19. QuickLZ compression level can only be set to level 1; no other values are available. Compression level with zlib can be set to values from 1 - 9. Compression level with RLE can be set to values from 1 - 4.

An `ENCODING` clause specifies compression type and level for individual columns. When an `ENCODING` clause conflicts with a `WITH` clause, the `ENCODING` clause has higher precedence than the `WITH` clause.

### <a id="im159764"></a>To create a compressed table 

The `WITH` clause of the `CREATE TABLE` command declares the table storage options. Tables that use compression must be append-optimized tables. For example, to create an append-optimized table with zlib compression at a compression level of 5:

```
=> CREATE TABLE foo (a int, b text) 
   WITH (appendoptimized=true, compresstype=zlib, compresslevel=5);

```

## <a id="topic41"></a>Checking the Compression and Distribution of an Append-Optimized Table 

Greenplum provides built-in functions to check the compression ratio and the distribution of an append-optimized table. The functions take either the object ID or a table name. You can qualify the table name with a schema name.

<table class="table" id="topic41__im161827"><caption><span class="table--title-label">Table 2. </span><span class="title">Functions for compressed append-optimized table metadata</span></caption><colgroup><col style="width:183pt"><col style="width:98pt"><col style="width:169pt"></colgroup><thead class="thead">
            <tr class="row">
              <th class="entry" id="topic41__im161827__entry__1">Function</th>
              <th class="entry" id="topic41__im161827__entry__2">Return Type</th>
              <th class="entry" id="topic41__im161827__entry__3">Description</th>
            </tr>
          </thead><tbody class="tbody">
            <tr class="row">
              <td class="entry" headers="topic41__im161827__entry__1">get_ao_distribution(name)<p class="p">get_ao_distribution(oid)</p></td>
              <td class="entry" headers="topic41__im161827__entry__2">Set of (dbid, tuplecount) rows</td>
              <td class="entry" headers="topic41__im161827__entry__3">Shows the distribution of an append-optimized table's rows
                across the array. Returns a set of rows, each of which includes a segment
                  <em class="ph i">dbid</em> and the number of tuples stored on the segment.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic41__im161827__entry__1">get_ao_compression_ratio(name)<p class="p">get_ao_compression_ratio(oid)</p></td>
              <td class="entry" headers="topic41__im161827__entry__2">float8</td>
              <td class="entry" headers="topic41__im161827__entry__3">Calculates the compression ratio for a compressed
                append-optimized table. If information is not available, this function returns a
                value of -1.</td>
            </tr>
          </tbody></table>

The compression ratio is returned as a common ratio. For example, a returned value of `3.19`, or `3.19:1`, means that the uncompressed table is slightly larger than three times the size of the compressed table.

The distribution of the table is returned as a set of rows that indicate how many tuples are stored on each segment. For example, in a system with four primary segments with *dbid* values ranging from 0 - 3, the function returns four rows similar to the following:

```
=# SELECT get_ao_distribution('lineitem_comp');
 get_ao_distribution
---------------------
(0,7500721)
(1,7501365)
(2,7499978)
(3,7497731)
(4 rows)

```

## <a id="topic42"></a>Support for Run-length Encoding 

Greenplum Database supports Run-length Encoding \(RLE\) for column-level compression. RLE data compression stores repeated data as a single data value and a count. For example, in a table with two columns, a date and a description, that contains 200,000 entries containing the value `date1` and 400,000 entries containing the value `date2`, RLE compression for the date field is similar to `date1 200000 date2 400000`. RLE is not useful with files that do not have large sets of repeated data as it can greatly increase the file size.

There are four levels of RLE compression available. The levels progressively increase the compression ratio, but decrease the compression speed.

Greenplum Database versions 4.2.1 and later support column-oriented RLE compression. To backup a table with RLE compression that you intend to restore to an earlier version of Greenplum Database, alter the table to have no compression or a compression type supported in the earlier version \(`ZLIB` or `QUICKLZ`\) before you start the backup operation.

Greenplum Database combines delta compression with RLE compression for data in columns of type `BIGINT`, `INTEGER`, `DATE`, `TIME`, or `TIMESTAMP`. The delta compression algorithm is based on the change between consecutive column values and is designed to improve compression when data is loaded in sorted order or when the compression is applied to data in sorted order.

## <a id="topic43"></a>Adding Column-level Compression 

You can add the following storage parameters to a column for append-optimized tables with column orientation:

-   Compression type
-   Compression level
-   Block size for a column

Add storage parameters using the `CREATE TABLE`, `ALTER TABLE`, and `CREATE TYPE` commands.

The following table details the types of storage parameters and possible values for each.

<table class="table" id="topic43__im198636"><caption><span class="table--title-label">Table 3. </span><span class="title">Storage Parameters for Column-level Compression</span></caption><colgroup><col style="width:87pt"><col style="width:95pt"><col style="width:147pt"><col style="width:167.25pt"></colgroup><thead class="thead">
            <tr class="row">
              <th class="entry" id="topic43__im198636__entry__1">Name</th>
              <th class="entry" id="topic43__im198636__entry__2">Definition</th>
              <th class="entry" id="topic43__im198636__entry__3">Values</th>
              <th class="entry" id="topic43__im198636__entry__4">Comment</th>
            </tr>
          </thead><tbody class="tbody">
            <tr class="row">
              <td class="entry" headers="topic43__im198636__entry__1">
                <code class="ph codeph">compresstype</code>
              </td>
              <td class="entry" headers="topic43__im198636__entry__2">Type of compression.</td>
              <td class="entry" headers="topic43__im198636__entry__3"><code class="ph codeph">zstd: </code>Zstandard
                    algorithm<p class="p"><code class="ph codeph">zlib: </code>deflate
                      algorithm</p><p class="p"><code class="ph codeph">quicklz</code>: fast
                    compression</p><p class="p"><code class="ph codeph">RLE_TYPE</code>: run-length encoding
                    </p><p class="p"><code class="ph codeph">none</code>: no compression</p></td>
              <td class="entry" headers="topic43__im198636__entry__4">Values are not case-sensitive.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic43__im198636__entry__1" rowspan="4">
                <code class="ph codeph">compresslevel</code>
              </td>
              <td class="entry" headers="topic43__im198636__entry__2" rowspan="4">Compression level.</td>
              <td class="entry" headers="topic43__im198636__entry__3"><code class="ph codeph">zlib</code> compression:
                  <code class="ph codeph">1</code>-<code class="ph codeph">9</code></td>
              <td class="entry" headers="topic43__im198636__entry__4"><code class="ph codeph">1</code> is the fastest method with the least
                compression. <code class="ph codeph">1</code> is the default.<p class="p"><code class="ph codeph">9</code> is the slowest
                  method with the most compression.</p></td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic43__im198636__entry__3"><code class="ph codeph">zstd</code> compression:
                  <code class="ph codeph">1</code>-<code class="ph codeph">19</code></td>
              <td class="entry" headers="topic43__im198636__entry__4"><code class="ph codeph">1</code> is the fastest method with the least
                compression. <code class="ph codeph">1</code> is the default.<p class="p"><code class="ph codeph">19</code> is the slowest
                  method with the most compression.</p></td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic43__im198636__entry__3"><code class="ph codeph">QuickLZ</code> compression:<p class="p"><code class="ph codeph">1</code> – use
                  compression</p></td>
              <td class="entry" headers="topic43__im198636__entry__4"><code class="ph codeph">1</code> is the default.</td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic43__im198636__entry__3"><code class="ph codeph">RLE_TYPE</code> compression: <code class="ph codeph">1</code> –
                  <code class="ph codeph">4</code><p class="p"><code class="ph codeph">1</code> - apply RLE only</p><p class="p"><code class="ph codeph">2</code>
                  - apply RLE then apply zlib compression level 1</p><p class="p"><code class="ph codeph">3</code> - apply
                  RLE then apply zlib compression level 5</p><p class="p"><code class="ph codeph">4</code> - apply RLE then
                  apply zlib compression level 9</p></td>
              <td class="entry" headers="topic43__im198636__entry__4"><code class="ph codeph">1</code> is the fastest method with the least
                    compression.<p class="p"><code class="ph codeph">4</code> is the slowest method with the most
                  compression. <code class="ph codeph">1</code> is the default.</p></td>
            </tr>
            <tr class="row">
              <td class="entry" headers="topic43__im198636__entry__1">
                <code class="ph codeph">blocksize</code>
              </td>
              <td class="entry" headers="topic43__im198636__entry__2">The size in bytes for each block in the table</td>
              <td class="entry" headers="topic43__im198636__entry__3">
                <code class="ph codeph">8192 – 2097152</code>
              </td>
              <td class="entry" headers="topic43__im198636__entry__4">The value must be a multiple of 8192.</td>
            </tr>
          </tbody></table>

The following is the format for adding storage parameters.

```
[ ENCODING ( <storage_parameter [,…] ) ] 
```

where the word ENCODING is required and the storage parameter has three parts:

-   The name of the parameter
-   An equals sign
-   The specification

Separate multiple storage parameters with a comma. Apply a storage parameter to a single column or designate it as the default for all columns, as shown in the following `CREATE TABLE` clauses.

*General Usage:*

```
<column_name> <data_type> ENCODING ( <storage_directive> [, … ] ), …  

```

```
COLUMN <column_name> ENCODING ( <storage_directive> [, … ] ), … 

```

```
DEFAULT COLUMN ENCODING ( <storage_directive> [, … ] )

```

*Example:*

```
C1 char ENCODING (compresstype=quicklz, blocksize=65536) 

```

```
COLUMN C1 ENCODING (compresstype=zlib, compresslevel=6, blocksize=65536)

```

```
DEFAULT COLUMN ENCODING (compresstype=quicklz)

```

### <a id="topic44"></a>Default Compression Values 

If the compression type, compression level and block size are not defined, the default is no compression, and the block size is set to the Server Configuration Parameter `block_size`.

### <a id="topic45"></a>Precedence of Compression Settings 

Column compression settings are inherited from the type level to the table level to the partition level to the subpartition level. The lowest-level settings have priority.

-   Column compression settings defined at the table level override any compression settings for the type.
-   Column compression settings specified at the table level override any compression settings for the entire table.
-   Column compression settings specified for partitions override any compression settings at the column or table levels.
-   Column compression settings specified for subpartitions override any compression settings at the partition, column or table levels.
-   When an `ENCODING` clause conflicts with a `WITH` clause, the `ENCODING` clause has higher precedence than the `WITH` clause.

> **Note** The `INHERITS` clause is not allowed in a table that contains a storage parameter or a column reference storage parameter.

Tables created using the `LIKE` clause ignore storage parameter and column reference storage parameters.

### <a id="topic46"></a>Optimal Location for Column Compression Settings 

The best practice is to set the column compression settings at the level where the data resides. See [Example 5](#topic52), which shows a table with a partition depth of 2. `RLE_TYPE` compression is added to a column at the subpartition level.

### <a id="topic47"></a>Storage Parameters Examples 

The following examples show the use of storage parameters in `CREATE TABLE` statements.

#### <a id="topic48"></a>Example 1 

In this example, column `c1` is compressed using `zstd` and uses the block size defined by the system. Column `c2` is compressed with `quicklz`, and uses a block size of `65536`. Column `c3` is not compressed and uses the block size defined by the system.

```
CREATE TABLE T1 (c1 int ENCODING (compresstype=zstd),
                  c2 char ENCODING (compresstype=quicklz, blocksize=65536),
                  c3 char)    WITH (appendoptimized=true, orientation=column);
```

#### <a id="topic49"></a>Example 2 

In this example, column `c1` is compressed using `zlib` and uses the block size defined by the system. Column `c2` is compressed with `quicklz`, and uses a block size of `65536`. Column `c3` is compressed using `RLE_TYPE` and uses the block size defined by the system.

```
CREATE TABLE T2 (c1 int ENCODING (compresstype=zlib),
                  c2 char ENCODING (compresstype=quicklz, blocksize=65536),
                  c3 char,
                  COLUMN c3 ENCODING (compresstype=RLE_TYPE)
                  )
    WITH (appendoptimized=true, orientation=column);
```

#### <a id="topic50"></a>Example 3 

In this example, column `c1` is compressed using `zlib` and uses the block size defined by the system. Column `c2` is compressed with `quicklz`, and uses a block size of `65536`. Column `c3` is compressed using `zlib` and uses the block size defined by the system. Note that column `c3` uses `zlib` \(not `RLE_TYPE`\) in the partitions, because the column storage in the partition clause has precedence over the storage parameter in the column definition for the table.

```
CREATE TABLE T3 (c1 int ENCODING (compresstype=zlib),
                  c2 char ENCODING (compresstype=quicklz, blocksize=65536),
                  c3 text, COLUMN c3 ENCODING (compresstype=RLE_TYPE) )
    WITH (appendoptimized=true, orientation=column)
    PARTITION BY RANGE (c3) (START ('1900-01-01'::DATE)          
                             END ('2100-12-31'::DATE),
                             COLUMN c3 ENCODING (compresstype=zlib));
```

#### <a id="topic51"></a>Example 4 

In this example, `CREATE TABLE` assigns the `zlib` `compresstype` storage parameter to `c1`. Column `c2` has no storage parameter and inherits the compression type \(`quicklz`\) and block size \(`65536`\) from the `DEFAULT COLUMN ENCODING` clause.

Column `c3`'s `ENCODING` clause defines its compression type, `RLE_TYPE`. The `ENCODING` clause defined for a specific column overrides the `DEFAULT ENCODING` clause, so column `c3` uses the default block size, `32768`.

Column `c4` has a compress type of `none` and uses the default block size.

```
CREATE TABLE T4 (c1 int ENCODING (compresstype=zlib),
                  c2 char,
                  c3 text,
                  c4 smallint ENCODING (compresstype=none),
                  DEFAULT COLUMN ENCODING (compresstype=quicklz,
                                             blocksize=65536),
                  COLUMN c3 ENCODING (compresstype=RLE_TYPE)
                  ) 
   WITH (appendoptimized=true, orientation=column);
```

#### <a id="topic52"></a>Example 5 

This example creates an append-optimized, column-oriented table, T5. T5 has two partitions, `p1` and `p2`, each of which has subpartitions. Each subpartition has `ENCODING` clauses:

-   The `ENCODING` clause for partition `p1`'s subpartition `sp1` defines column `i`'s compression type as `zlib` and block size as 65536.
-   The `ENCODING` clauses for partition `p2`'s subpartition `sp1` defines column `i`'s compression type as `rle_type` and block size is the default value. Column `k` uses the default compression and its block size is 8192.

    ```
    CREATE TABLE T5(i int, j int, k int, l int) 
        WITH (appendoptimized=true, orientation=column)
        PARTITION BY range(i) SUBPARTITION BY range(j)
        (
           partition p1 start(1) end(2)
           ( subpartition sp1 start(1) end(2) 
             column i encoding(compresstype=zlib, blocksize=65536)
           ), 
           partition p2 start(2) end(3)
           ( subpartition sp1 start(1) end(2)
               column i encoding(compresstype=rle_type)
               column k encoding(blocksize=8192)
           )
        );
    ```


For an example showing how to add a compressed column to an existing table with the `ALTER TABLE` command, see [Adding a Compressed Column to Table](#topic60).

### <a id="topic53"></a>Adding Compression in a TYPE Command 

When you create a new type, you can define default compression attributes for the type. For example, the following `CREATE TYPE` command defines a type named `int33` that specifies `quicklz` compression.

First, you must define the input and output functions for the new type, `int33_in` and `int33_out`:

```
CREATE FUNCTION int33_in(cstring) RETURNS int33
  STRICT IMMUTABLE LANGUAGE internal AS 'int4in';
CREATE FUNCTION int33_out(int33) RETURNS cstring
  STRICT IMMUTABLE LANGUAGE internal AS 'int4out';
```

Next, you define the type named `int33`:

```
CREATE TYPE int33 (
   internallength = 4,
   input = int33_in,
   output = int33_out,
   alignment = int4,
   default = 123,
   passedbyvalue,
   compresstype="zlib",
   blocksize=65536,
   compresslevel=1
   );
```

When you specify `int33` as a column type in a `CREATE TABLE` command, the column is created with the storage parameters you specified for the type:

```
CREATE TABLE t2 (c1 int33)
    WITH (appendoptimized=true, orientation=column);
```

Table- or column- level storage attributes that you specify in a table definition override type-level storage attributes. For information about creating and adding compression attributes to a type, see [CREATE TYPE](../../ref_guide/sql_commands/CREATE_TYPE.html). For information about changing compression specifications in a type, see [ALTER TYPE](../../ref_guide/sql_commands/ALTER_TYPE.html).

#### <a id="topic54"></a>Choosing Block Size 

The blocksize is the size, in bytes, for each block in a table. Block sizes must be between 8192 and 2097152 bytes, and be a multiple of 8192. The default is 32768.

Specifying large block sizes can consume large amounts of memory. Block size determines buffering in the storage layer. Greenplum maintains a buffer per partition, and per column in column-oriented tables. Tables with many partitions or columns consume large amounts of memory.

## <a id="topic55"></a>Altering a Table 

The `ALTER TABLE` command changes the definition of a table. Use `ALTER TABLE` to change table attributes such as column definitions, distribution policy, access method, storage parameters, and partition structure \(see also [Maintaining Partitioned Tables](ddl-partition.html)\). For example, to add a not-null constraint to a table column:

```
=> ALTER TABLE address ALTER COLUMN street SET NOT NULL;

```

### <a id="topic56"></a>Altering Table Distribution 

`ALTER TABLE` provides options to change a table's distribution policy. When the table distribution options change, the table data may be redistributed on disk, which can be resource intensive. You can also redistribute table data using the existing distribution policy.

### <a id="topic57"></a>Changing the Distribution Policy 

For partitioned tables, changes to the distribution policy apply recursively to the child partitions. This operation preserves the ownership and all other attributes of the table. For example, the following command redistributes the table sales across all segments using the customer\_id column as the distribution key:

```
ALTER TABLE sales SET DISTRIBUTED BY (customer_id); 

```

When you change the hash distribution of a table, table data is automatically redistributed. Changing the distribution policy to a random distribution does not cause the data to be redistributed. For example, the following `ALTER TABLE` command has no immediate effect:

```
ALTER TABLE sales SET DISTRIBUTED RANDOMLY;

```

Changing the distribution policy of a table to `DISTRIBUTED REPLICATED` or from `DISTRIBUTED REPLICATED` automatically redistributes the table data.

### <a id="topic58"></a>Redistributing Table Data 

To redistribute table data for tables with a random distribution policy \(or when the hash distribution policy has not changed\) use `REORGANIZE=TRUE`. Reorganizing data may be necessary to correct a data skew problem, or when segment resources are added to the system. For example, the following command redistributes table data across all segments using the current distribution policy, including random distribution.

```
ALTER TABLE sales SET WITH (REORGANIZE=TRUE);

```

Changing the distribution policy of a table to `DISTRIBUTED REPLICATED` or from `DISTRIBUTED REPLICATED` always redistributes the table data, even when you use `REORGANIZE=FALSE`.

### <a id="access_method"></a>Altering the Table Access Method

You may alter the method for accessing a table using the `SET ACCESS METHOD` clause. Set to `heap` to alter the table to be a heap-storage table, `ao_row` to alter the table to be append-optimized with row-oriented storage (AO), or `ao_column` to alter the table to be append-optimized with column-oriented storage (AOCO).

  <p class="note">
<strong>Note:</strong>
Although you can specify the table's access method using <code>SET &lt;storage_parameter></code> or  <code>SET WITH&lt;storage_parameter></code>, VMware recommends that you use <code>SET ACCESS METHOD &lt;access_method></code> instead.
</p>

### <a id="topic59"></a>Altering the Table Storage Model 

You may dynamically update a table's storage model -- including whether the table is heap, AO or AOCO; the table's compression and blocksize settings; and the table's fillfactor; --  by setting a variety of storage parameters when you invoke `ALTER TABLE` with the `SET <storage_parameter>` clause. This is true for both regular tables and partitioned tables.

#### <a id="storage_model_partition"></a>Inheritance Rules for Altering a Partitioned Table's Storage Model

The following inheritance rules apply to the storage model of a partitioned table:

- Altering the storage model at the partition root changes the storage model for all existing children and all future children.

- Altering the storage model at the partition root with the `ONLY` keyword changes the storage model only for all future children.

- Altering the storage model at a leaf changes the storage model only for that leaf.


#### <a id="topic60"></a>Adding a Compressed Column to Table 

Use `ALTER TABLE` command to add a compressed column to a table. All of the options and constraints for compressed columns described in [Adding Column-level Compression](#topic43) apply to columns added with the `ALTER TABLE` command.

The following example shows how to add a column with `zlib` compression to a table, `T1`.

```
ALTER TABLE T1
      ADD COLUMN c4 int DEFAULT 0
      ENCODING (compresstype=zlib);

```

#### <a id="topic61"></a>Inheritance of Compression Settings 

A partition added to a table that has subpartitions defined with compression settings inherits the compression settings from the subpartition. The following example shows how to create a table with subpartition encodings, then alter it to add a partition.

```
CREATE TABLE ccddl (i int, j int, k int, l int)
  WITH
    (appendoptimized = TRUE, orientation=COLUMN)
  PARTITION BY range(j)
  SUBPARTITION BY list (k)
  SUBPARTITION template(
    SUBPARTITION sp1 values(1, 2, 3, 4, 5),
    COLUMN i ENCODING(compresstype=ZLIB),
    COLUMN j ENCODING(compresstype=QUICKLZ),
    COLUMN k ENCODING(compresstype=ZLIB),
    COLUMN l ENCODING(compresstype=ZLIB))
  (PARTITION p1 START(1) END(10),
   PARTITION p2 START(10) END(20))
;

ALTER TABLE ccddl
  ADD PARTITION p3 START(20) END(30)
;

```

Running the `ALTER TABLE` command creates partitions of table `ccddl` named `ccddl_1_prt_p3` and `ccddl_1_prt_p3_2_prt_sp1`. Partition `ccddl_1_prt_p3` inherits the different compression encodings of subpartition `sp1`.

## <a id="topic62"></a>Dropping a Table 

The`DROP TABLE`command removes tables from the database. For example:

```
DROP TABLE mytable;

```

To empty a table of rows without removing the table definition, use `DELETE` or `TRUNCATE`. For example:

```
DELETE FROM mytable;

TRUNCATE mytable;

```

`DROP TABLE`always removes any indexes, rules, triggers, and constraints that exist for the target table. Specify `CASCADE`to drop a table that is referenced by a view. `CASCADE` removes dependent views.

