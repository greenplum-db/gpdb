---
title: Enabling Compression 
---

You can configure Greenplum Database to use data compression with some database features and with some utilities.Compression reduces disk usage and improves I/O across the system, however, it adds some performance overhead when compressing and decompressing data.

You can configure support for data compression with these features and utilities. See the specific feature or utility for information about support for compression.

-   Append-optimized tables support compressing table data. See [CREATE TABLE](../../ref_guide/sql_commands/CREATE_TABLE.html).
-   User-defined data types can be defined to compress data. See [CREATE TYPE](../../ref_guide/sql_commands/CREATE_TYPE.html).
-   The external table protocols [gpfdist](../external/g-gpfdist-protocol.html) \([gpfdists](../external/g-gpfdists-protocol.html)\), [s3](../external/g-s3-protocol.html), and [pxf](../external/pxf-overview.html) support compression when accessing external data. For information about external tables, see [CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html).
-   Workfiles \(temporary spill files that are created when running a query that requires more memory than it is allocated\) can be compressed. See the server configuration parameter [gp\_workfile\_compression](../../ref_guide/config_params/guc-list.html).
-   The Greenplum Database utilities [gpbackup](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html), [gprestore](https://docs.vmware.com/en/VMware-Greenplum-Backup-and-Restore/index.html), [gpcopy](../../utility_guide/ref/gpcopy.html), [gpload](../../utility_guide/ref/gpload.html), and [gplogfilter](../../utility_guide/ref/gplogfilter.html) support compression.

For some compression algorithms \(such as zlib\) Greenplum Database requires software packages installed on the host system. For information about required software packages, see the *Greenplum Database Installation Guide*.

**Parent topic:** [Managing a Greenplum System](../managing/partII.html)

