---
title: gpfdist:// Protocol 
---

The `gpfdist://` protocol is used in a URI to reference a running `gpfdist` instance.

The [gpfdist](../../utility_guide/ref/gpfdist.html) utility serves external data files from a directory on a file host to all Greenplum Database segments in parallel.

`gpfdist` is located in the `$GPHOME/bin` directory on your Greenplum Database master host and on each segment host.

Run `gpfdist` on the host where the external data files reside. For readable external tables, `gpfdist` uncompresses `gzip` \(`.gz`\) and `bzip2` \(.`bz2`\) files automatically. For writable external tables, data is compressed using `gzip` if the target file has a `.gz` extension. You can use the wildcard character \(\*\) or other C-style pattern matching to denote multiple files to read. The files specified are assumed to be relative to the directory that you specified when you started the `gpfdist` instance.

**Note:** Compression is not supported for readable and writeable external tables when the `gpfdist` utility runs on Windows platforms.

All primary segments access the external file\(s\) in parallel, subject to the number of segments set in the `gp_external_max_segments` server configuration parameter. Use multiple `gpfdist` data sources in a `CREATE EXTERNAL TABLE` statement to scale the external table's scan performance.

`gpfdist` supports data transformations. You can write a transformation process to convert external data from or to a format that is not directly supported with Greenplum Database external tables.

For more information about configuring `gpfdist`, see [Using the Greenplum Parallel File Server \(gpfdist\)](g-using-the-greenplum-parallel-file-server--gpfdist-.html).

See the `gpfdist` reference documentation for more information about using `gpfdist` with external tables.

**Parent topic:**[Defining External Tables](../external/g-external-tables.html)

