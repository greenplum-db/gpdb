# pg_appendonly 

The `pg_appendonly` table contains information about the storage options and other characteristics of append-optimized tables.

|column|type|references|description|
|------|----|----------|-----------|
|`relid`|oid| |The table object identifier \(OID\) of the compressed table.|
|`blocksize`|integer| |Block size used for compression of append-optimized tables. Valid values are 8K - 2M. Default is `32K`.|
|`safefswritesize`|integer| |Minimum size for safe write operations to append-optimized tables in a non-mature file system. Commonly set to a multiple of the extent size of the file system; for example, Linux ext3 is 4096 bytes, so a value of 32768 is commonly used.|
|`compresslevel`|smallint| |The compression level, with compression ratio increasing from 1 to 19. When `quicklz`<sup>1</sup> is specified for compresstype, valid values are 1 or 3. With `zlib` specified, valid values are 1-9. When `zstd` is specified, valid values are 1-19.|
|`majorversion`|smallint| |The major version number of the pg\_appendonly table.|
|`minorversion`|smallint| |The minor version number of the pg\_appendonly table.|
|`checksum`|boolean| |A checksum value that is stored to compare the state of a block of data at compression time and at scan time to ensure data integrity.|
|`compresstype`|text| |Type of compression used to compress append-optimized tables. Valid values are:<br/><br/>-   `none` \(no compression\)<br/><br/>-   `rle_type` \(run-length encoding compression\)<br/><br/>-   `zlib` \(gzip compression\)<br/><br/>-   `zstd` \(Zstandard compression\)<br/><br/>-   `quicklz`<sup>1</sup>|
|`columnstore`|boolean| |`1` for column-oriented storage, `0` for row-oriented storage.|
|`segrelid`|oid| |Table on-disk segment file id.|
|`segidxid`|oid| |Index on-disk segment file id.|
|`blkdirrelid`|oid| |Block used for on-disk column-oriented table file.|
|`blkdiridxid`|oid| |Block used for on-disk column-oriented index file.|
|`visimaprelid`|oid| |Visibility map for the table.|
|`visimapidxid`|oid| |B-tree index on the visibility map.|

> **Note** <sup>1</sup>QuickLZ compression is available only in the commercial release of VMware Greenplum.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

