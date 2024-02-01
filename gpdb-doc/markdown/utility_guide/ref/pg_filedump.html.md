# pg_filedump 

Formats VMware Greenplum data files into human-readable form.

## <a id="synopsis"></a>Synopsis 

```
pg_filedump [<option> ...] <filename>

pg_filedump -? | -h | --help

pg_filedump -V | --version
```

## <a id="desc"></a>Description 

The `pg_filedump` utility formats VMware Greenplum data files -- including table, index and control files -- into a human-readable format. 

To use `pg_filedump`, you must have:

- `gpsupport` 1.0.3 or higher installed 
- a search path that includes the `gpsupport` executable path

>**NOTE**
>`pg_filedump` is currently only supported for Greenplum 7 data files.

## <a id="options"></a>Options 

### <a id="options_heap_index"></a>Options for Heap and Index Files

 -a  
 :   Optional. Display absolute addresses when formatting (Block header information is always block-relative).

-b  
:   Optional. Display binary block images within a range. (This option disables all formatting options.) 
      
-d  
:   Optional. Display formatted block content dump (This option disables all formatting options.)

-D 
: Optional. Decode tuples using given comma separated list of types. Supported types include `bigint`, `bigserial`, `bool`, `char`, `charN`, `date`, `float`, `float4`, `float8`, `int`, `json`, `macaddr`, `name`, `oid`, `real`, `serial`, `smallint`, `smallserial`, `text`, `time`, `timestamp`, `timetz`, `uuid`, `varchar`, `varcharN`, `xid`, `xml`.

Any attributes in the tuple not specified by `D` arguments will not be printed.

-f   
:   Optional. Display formatted block content dump along with interpretation.

-i  
:   Optional. Display interpreted item details.

-k  
:   Optional. Verify block checksums.

-o  
:   Optional. Do not dump old values. 

-R  [<startblock>] [<endblock>]
:   Optional. Display specific block ranges within the file; blocks are indexed from `0`.

where <startblock> is the block to start at and <endblock> is the block to end at.

>**NOTE**
>If you pass <startblock> without also passing <endblock>, the `-R` option will from the starting block until the end of the file.

-s  <segsize>
:   Optional. Force segment size to <segsize>.

-t  
:   Optional. Dump TOAST files.

-v 
:   Optional. Output additional information about TOAST relations.

-n  <segnumber>
:   Optional. Force segment number to <segnumber>.

-S  
:   Optional. Force block size to <blocksize>.

-x  
:   Optional. Force interpreted formatting of block items as index items.
  
-B  
:   Force interpreted formatting of block items as bitmap index items.

-y  
:   Force interpreted formatting of block items as heap items.


### <a id="options_control"></a>Options for Control Files

  -c  
  :   Optional. Interpret the file listed as a control file.

  -f  
  :   Optional. Display formatted content dump along with interpretation

  -S  <blocksize> 
  :   Optional. Force block size to <blocksize>. 



### <a id="options_aoco"></a>Options for Append-Optimized Column-Oriented Table Files

  -z  
  :   Optional. Interpret the file listed as an append-optimized file.

  -T  
  :   Optional. Specify the compression type (`zlib`, `zstd`, `quicklz` or 
      none). If not specified, defaults to none.  

  -L  
  :   Optional. Specify the compression level (`1`, `2`, `3`, or `4`). If not 
      specified, defaults to `0`.

  -M  
  :   Optional. Checksum option for append-optimized files; by default, the utility  considers checksums not present

  -O  
  :   Optional. Specifies orientation of the append-optimized table (either `row` or `column`); if not specified, defaults to `row`.

## <a id="examples"></a>Examples 

Display the contents of a heap relation file:

```
pg_filedump <filename>
```

Display the contents of a heap relation file; the table in questions has two columns of type `int`: 

```
pg_filedump -D int,int <filename>
```

Display the contents of an append-optimized, row-oriented relation file:

```
pg_filedump -z -O row <filename>
```

Display the contents of a control file:

```
pg_filedump -c <filename>
```