# Specify gphdfs Protocol in an External Table Definition \(Deprecated\) 

The `gphdfs` `LOCATION` clause of the `CREATE EXTERNAL TABLE` command for HDFS files differs slightly for Hadoop HA \(High Availability\) clusters, Hadoop clusters without HA, and MapR clusters.

In a Hadoop HA cluster, the `LOCATION` clause references the logical nameservices id \(the `dfs.nameservices` property in the `hdfs-site.xml` configuration file\). The `hdfs-site.xml` file with the nameservices configuration must be installed on the Greenplum master and on each segment host.

For example, if `dfs.nameservices` is set to `mycluster` the `LOCATION` clause takes this format:

```
LOCATION ('gphdfs://mycluster/path/filename.txt')
```

A cluster without HA specifies the hostname and port of the name node in the `LOCATION` clause:

```
LOCATION ('gphdfs://hdfs_host[:port]/path/filename.txt')
```

If you are using MapR clusters, you specify a specific cluster and the file:

-   To specify the default cluster, the first entry in the MapR configuration file `/opt/mapr/conf/mapr-clusters.conf`, specify the location of your table with this syntax:

    ```
    LOCATION ('gphdfs:///<file_path>')
    ```

    The file\_path is the path to the file.

-   To specify another MapR cluster listed in the configuration file, specify the file with this syntax:

    ```
    LOCATION ('gphdfs:///mapr/<cluster_name>/<file_path>')
    ```

    The cluster\_name is the name of the cluster specified in the configuration file and file\_path is the path to the file.


For information about MapR clusters, see the MapR documentation.

Restrictions for `HDFS` files are as follows.

-   You can specify one path for a readable external table with `gphdfs`. Wildcard characters are allowed. If you specify a directory, the default is all files in the directory.

    You can specify only a directory for writable external tables.

-   The URI of the `LOCATION` clause cannot contain any of these four characters: `\`, `'`, `<`, `>`. The `CREATE EXTERNAL TABLE` returns a an error if the URI contains any of the characters.
-   Format restrictions are as follows.
    -   Only the `gphdfs_import` formatter is allowed for readable external tables with a custom format.
    -   Only the `gphdfs_export` formatter is allowed for writable external tables with a custom format.
    -   You can set compression only for writable external tables. Compression settings are automatic for readable external tables.

**Parent topic:** [Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)

## Setting Compression Options for Hadoop Writable External Tables 

Compression options for Hadoop Writable External Tables use the form of a URI query and begin with a question mark. Specify multiple compression options with an ampersand \(`&`\).

|Compression Option|Values|Default Value|
|------------------|------|-------------|
|compress|`true` or `false`|`false`|
|compression\_type|`BLOCK` or `RECORD`|`RECORD`<br/><br/>For `AVRO` format, `compression_type` must be `block` if `compress` is `true`.|
|codec|Codec class name|`GzipCodec` for `text` format and `DefaultCodec` for `gphdfs_export` format.<br/><br/>For `AVRO` format, the value is either `deflate` \(the default\) or `snappy`.|
|codec\_level \(for `AVRO` format and `deflate` codec only\)|integer between 1 and 9|`6`<br/><br/>The level controls the trade-off between speed and compression. Valid values are 1 to 9, where 1 is the fastest and 9 is the most compressed.|

Place compression options in the query portion of the URI.

