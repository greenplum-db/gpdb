# pageinspect 

The `pageinspect` module provides functions for low level inspection of the contents of database pages. `pageinspect` is available only to Greenplum Database superusers.

The Greenplum Database `pageinspect` module is based on the PostgreSQL `pageinspect` module. The Greenplum version of the module differs as described in the [Greenplum Database Considerations](#topic_gp) topic.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `pageinspect` module is installed when you install Greenplum Database. Before you can use any of the functions defined in the module, you must register the `pageinspect` extension in each database in which you want to use the functions:

```
CREATE EXTENSION pageinspect;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_upgrade"></a>Upgrading the Module 

If you are currently using `pageinspect` in your Greenplum installation and you want to access newly-released module functionality, you must update the `pageinspect` extension in every database in which it is currently registered:

```
ALTER EXTENSION pageinspect UPDATE;
```

## <a id="topic_info"></a>Module Documentation 

See [pageinspect](https://www.postgresql.org/docs/12/pageinspect.html) in the PostgreSQL documentation for detailed information about the majority of functions in this module.

The next topic includes documentation for Greenplum-added `pageinspect` functions.

## <a id="topic_gp"></a>Greenplum Database Considerations 

When using this module with Greenplum Database, consider the following:

-   The Greenplum Database version of the `pageinspect` does not allow inspection of pages belonging to append-optimized or external relations.
-   For `pageinspect` functions that read data from a database, the function reads data only from the segment instance where the function is run. For example, the `get_raw_page()` function returns a `block number out of range` error when you try to read data from a user-defined table on the Greenplum Database coordinator because there is no data in the table on the coordinator segment. The function will read data from a system catalog table on the coordinator segment.

### <a id="gp_funcs"></a>Greenplum-Added Functions 

In addition to the functions specified in the PostgreSQL documentation, Greenplum Database provides these additional `pageinspect` functions for inspecting bitmap index pages:

|Function Name|Description|
|-------------|-----------|
|bm\_metap\(relname text\) returns record|Returns information about a bitmap index's meta page.|
|bm\_bitmap\_page\_header\(relname text, blkno int\) returns record|Returns the header information for a bitmap page; this corresponds to the opaque section from the page header.|
|bm\_lov\_page\_items\(relname text, blkno int\) returns setof record|Returns the list of value \(LOV\) items present in a bitmap LOV page.|
|bm\_bitmap\_page\_items\(relname text, blkno int\) returns setof record|Returns the content words and their compression statuses for a bitmap page.|
|bm\_bitmap\_page\_items\(page bytea\) returns setof record|Returns the content words and their compression statuses for a page image obtained by `get_raw_page()`.|

### <a id="topic_examples"></a>Examples 

Greenplum-added `pageinspect` function usage examples follow.

Obtain information about the meta page of the bitmap index named `i1`:

```
testdb=# SELECT * FROM bm_metap('i1');
   magic    | version | auxrelid | auxindexrelid | lovlastblknum
------------+---------+----------+---------------+---------------
 1112101965 |       2 |   169980 |        169982 |             1
(1 row)
```

Display the header information for the second block of the bitmap index named `i1`:

```
testdb=# SELECT * FROM bm_bitmap_page_header('i1', 2);
 num_words | next_blkno | last_tid 
-----------+------------+----------
 3         | 4294967295 | 65536    
(1 row)
```

Display the LOV items located in the first block of the bitmap index named `i1`:

```
testdb=# SELECT * FROM bm_lov_page_items('i1', 1) ORDER BY itemoffset;
 itemoffset | lov_head_blkno | lov_tail_blkno | last_complete_word      | last_word               | last_tid | last_setbit_tid | is_last_complete_word_fill | is_last_word_fill 
------------+----------------+----------------+-------------------------+-------------------------+----------+-----------------+----------------------------+-------------------
 1          | 4294967295     | 4294967295     | ff ff ff ff ff ff ff ff | 00 00 00 00 00 00 00 00 | 0        | 0               | f                          | f                 
 2          | 2              | 2              | 80 00 00 00 00 00 00 01 | 00 00 00 00 07 ff ff ff | 65600    | 65627           | t                          | f                 
 3          | 3              | 3              | 80 00 00 00 00 00 00 02 | 00 3f ff ff ff ff ff ff | 131200   | 131254          | t                          | f                 
(3 rows)
```

Return the content words located in the second block of the bitmap index named `i1`:

```
testdb=# SELECT * FROM bm_bitmap_page_items('i1', 2) ORDER BY word_num;
 word_num | compressed | content_word            
----------+------------+-------------------------
 0        | t          | 80 00 00 00 00 00 00 0e 
 1        | f          | 00 00 00 00 00 00 1f ff 
 2        | t          | 00 00 00 00 00 00 03 f1 
(3 rows)
```

Alternatively, return the content words located in the heap page image of the same bitmap index and block:

```
testdb=# SELECT * FROM bm_bitmap_page_items(get_raw_page('i1', 2)) ORDER BY word_num;
 word_num | compressed | content_word            
----------+------------+-------------------------
 0        | t          | 80 00 00 00 00 00 00 0e 
 1        | f          | 00 00 00 00 00 00 1f ff 
 2        | t          | 00 00 00 00 00 00 03 f1 
(3 rows)
```

