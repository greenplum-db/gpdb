---
title: s3:// Protocol 
---

The `s3` protocol is used in a URL that specifies the location of an Amazon S3 bucket and a prefix to use for reading or writing files in the bucket.

Amazon Simple Storage Service \(Amazon S3\) provides secure, durable, highly-scalable object storage. For information about Amazon S3, see [Amazon S3](https://aws.amazon.com/s3/).

You can define read-only external tables that use existing data files in the S3 bucket for table data, or writable external tables that store the data from INSERT operations to files in the S3 bucket. Greenplum Database uses the S3 URL and prefix specified in the protocol URL either to select one or more files for a read-only table, or to define the location and filename format to use when uploading S3 files for `INSERT` operations to writable tables.

The `s3` protocol also supports [Dell EMC Elastic Cloud Storage](https://www.emc.com/en-us/storage/ecs/index.htm) \(ECS\), an Amazon S3 compatible service.

**Note:** The `pxf` protocol can access data in S3 and other object store systems such as Azure, Google Cloud Storage, and Minio. The `pxf` protocol can also access data in external Hadoop systems \(HDFS, Hive, HBase\), and SQL databases. See [pxf:// Protocol](g-pxf-protocol.html).

This topic contains the sections:

-   [Configuring the s3 Protocol](#s3_prereq)
-   [Using s3 External Tables](#s3_using)
-   [About the s3 Protocol LOCATION URL](#section_stk_c2r_kx)
-   [About Reading and Writing S3 Data Files](#section_c2f_zvs_3x)
-   [s3 Protocol AWS Server-Side Encryption Support](#s3_serversideencrypt)
-   [s3 Protocol Proxy Support](#s3_proxy)
-   [About Providing the S3 Authentication Credentials](#s3_auth)
-   [About the s3 Protocol Configuration File](#s3_config_file)
-   [About Specifying the Configuration File Location](#s3_config_param)
-   [s3 Protocol Limitations](#section_tsq_n3t_3x)
-   [Using the gpcheckcloud Utility](#s3chkcfg_utility)

## <a id="s3_prereq"></a>Configuring the s3 Protocol 

You must configure the `s3` protocol before you can use it. Perform these steps in each database in which you want to use the protocol:

1.  Create the read and write functions for the `s3` protocol library:

    ```
    CREATE OR REPLACE FUNCTION write_to_s3() RETURNS integer AS
       '$libdir/gps3ext.so', 's3_export' LANGUAGE C STABLE;
    ```

    ```
    CREATE OR REPLACE FUNCTION read_from_s3() RETURNS integer AS
       '$libdir/gps3ext.so', 's3_import' LANGUAGE C STABLE;
    ```

2.  Declare the `s3` protocol and specify the read and write functions you created in the previous step:

    ```
    CREATE PROTOCOL s3 (writefunc = write_to_s3, readfunc = read_from_s3);
    ```

    **Note:** The protocol name `s3` must be the same as the protocol of the URL specified for the external table that you create to access an S3 resource.

    The corresponding function is called by every Greenplum Database segment instance.


## <a id="s3_using"></a>Using s3 External Tables 

Follow these basic steps to use the `s3` protocol with Greenplum Database external tables. Each step includes links to relevant topics from which you can obtain more information. See also [s3 Protocol Limitations](#section_tsq_n3t_3x) to better understand the capabilities and limitations of s3 external tables:

1.  [Configure the s3 Protocol](#s3_prereq).
2.  Create the `s3` protocol configuration file:
    1.  Create a template `s3` protocol configuration file using the `gpcheckcloud` utility:

        ```
        gpcheckcloud -t > ./mytest_s3.config
        ```

    2.  (Optional) Edit the template file to specify the `accessid` and `secret` authentication credentials required to connect to the S3 location. See [About Providing the S3 Authentication Credentials](#s3_auth) and [About the s3 Protocol Configuration File](#s3_config_file) for information about specifying these and other `s3` protocol configuration parameters.
3.  Greenplum Database can access an `s3` protocol configuration file when the file is located on each segment host or when the file is served up by an `http/https` server. Identify where you plan to locate the configuration file, and note the location and configuration option \(if applicable\). Refer to [About Specifying the Configuration File Location](#s3_config_param) for more information about the location options for the file.

    If you are relying on the AWS credential file to authenticate, this file must reside at `~/.aws/credentials` on each Greenplum Database segment host.
4.  Use the `gpcheckcloud` utility to validate connectivity to the S3 bucket. You must specify the S3 endpoint name and bucket that you want to check.

    For example, if the `s3` protocol configuration file resides in the default location, you would run the following command:

    ```
    gpcheckcloud -c "s3://<s3-endpoint>/<s3-bucket>"
    ```

    `gpcheckcloud` attempts to connect to the S3 endpoint and lists any files in the S3 bucket, if available. A successful connection ends with the message:

    ```
    Your configuration works well.
    ```

    You can optionally use `gpcheckcloud` to validate uploading to and downloading from the S3 bucket. Refer to [Using the gpcheckcloud Utility](#s3chkcfg_utility) for information about this utility and other usage examples.

5.  Create an s3 external table by specifying an `s3` protocol URL in the `CREATE EXTERNAL TABLE` command, `LOCATION` clause.

    For read-only s3 tables, the URL defines the location and prefix used to select existing data files that comprise the s3 table. For example:

    ```
    CREATE READABLE EXTERNAL TABLE S3TBL (date text, time text, amt int)
       LOCATION('s3://s3-us-west-2.amazonaws.com/s3test.example.com/dataset1/normal/ config=/home/gpadmin/aws_s3/s3.conf')
       FORMAT 'csv';
    ```

    For writable s3 tables, the protocol URL defines the S3 location in which Greenplum Database writes the data files that back the table for `INSERT` operations. You can also specify a prefix that Greenplum will add to the files that it creates. For example:

    ```
    CREATE WRITABLE EXTERNAL TABLE S3WRIT (LIKE S3TBL)
       LOCATION('s3://s3-us-west-2.amazonaws.com/s3test.example.com/dataset1/normal/ config=/home/gpadmin/aws_s3/s3.conf')
       FORMAT 'csv';
    ```

    Refer to [About the s3 Protocol LOCATION URL](#section_stk_c2r_kx) for more information about the `s3` protocol URL.


## <a id="section_stk_c2r_kx"></a>About the s3 Protocol LOCATION URL 

When you use the `s3` protocol, you specify an S3 file location and optional configuration file location and region parameters in the `LOCATION` clause of the `CREATE EXTERNAL TABLE` command. The syntax follows:

```
's3://<S3_endpoint>[:<port>]/<bucket_name>/[<S3_prefix>] [region=<S3_region>] [config=<config_file_location> | config_server=<url>] [section=<section_name>]'
```

The `s3` protocol requires that you specify the S3 endpoint and S3 bucket name. Each Greenplum Database segment host must have access to the S3 location. The optional S3\_prefix value is used to select files for read-only S3 tables, or as a filename prefix to use when uploading files for s3 writable tables.

**Note:** The Greenplum Database `s3` protocol URL must include the S3 endpoint hostname.

To specify an ECS endpoint \(an Amazon S3 compatible service\) in the `LOCATION` clause, you must set the `s3` protocol configuration file parameter `version` to `2`. The `version` parameter controls whether the `region` parameter is used in the `LOCATION` clause. You can also specify an Amazon S3 location when the `version` parameter is 2. For information about the `version` parameter, see [About the s3 Protocol Configuration File](#s3_config_file).

**Note:** Although the S3\_prefix is an optional part of the syntax, you should always include an S3 prefix for both writable and read-only s3 tables to separate datasets as part of the [CREATE EXTERNAL TABLE](../../ref_guide/sql_commands/CREATE_EXTERNAL_TABLE.html) syntax.

For writable s3 tables, the `s3` protocol URL specifies the endpoint and bucket name where Greenplum Database uploads data files for the table. The S3 file prefix is used for each new file uploaded to the S3 location as a result of inserting data to the table. See [About Reading and Writing S3 Data Files](#section_c2f_zvs_3x).

For read-only s3 tables, the S3 file prefix is optional. If you specify an S3\_prefix, then the `s3` protocol selects all files that start with the specified prefix as data files for the external table. The `s3` protocol does not use the slash character \(`/`\) as a delimiter, so a slash character following a prefix is treated as part of the prefix itself.

For example, consider the following 5 files that each have the S3\_endpoint named `s3-us-west-2.amazonaws.com` and the bucket\_name `test1`:

```
s3://s3-us-west-2.amazonaws.com/test1/abc
s3://s3-us-west-2.amazonaws.com/test1/abc/
s3://s3-us-west-2.amazonaws.com/test1/abc/xx
s3://s3-us-west-2.amazonaws.com/test1/abcdef
s3://s3-us-west-2.amazonaws.com/test1/abcdefff
```

-   If the S3 URL is provided as `s3://s3-us-west-2.amazonaws.com/test1/abc`, then the `abc` prefix selects all 5 files.
-   If the S3 URL is provided as `s3://s3-us-west-2.amazonaws.com/test1/abc/`, then the `abc/` prefix selects the files `s3://s3-us-west-2.amazonaws.com/test1/abc/` and `s3://s3-us-west-2.amazonaws.com/test1/abc/xx`.
-   If the S3 URL is provided as `s3://s3-us-west-2.amazonaws.com/test1/abcd`, then the `abcd` prefix selects the files `s3://s3-us-west-2.amazonaws.com/test1/abcdef` and `s3://s3-us-west-2.amazonaws.com/test1/abcdefff`

Wildcard characters are not supported in an S3\_prefix; however, the S3 prefix functions as if a wildcard character immediately followed the prefix itself.

All of the files selected by the S3 URL \(S3\_endpoint/bucket\_name/S3\_prefix\) are used as the source for the external table, so they must have the same format. Each file must also contain complete data rows. A data row cannot be split between files.

For information about the Amazon S3 endpoints see [http://docs.aws.amazon.com/general/latest/gr/rande.html\#s3\_region](http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region). For information about S3 buckets and folders, see the Amazon S3 documentation [https://aws.amazon.com/documentation/s3/](https://aws.amazon.com/documentation/s3/). For information about the S3 file prefix, see the Amazon S3 documentation [Listing Keys Hierarchically Using a Prefix and Delimiter](http://docs.aws.amazon.com/AmazonS3/latest/dev/ListingKeysHierarchy.html).

You use the `config` or `config_server` parameter to specify the location of the required `s3` protocol configuration file that contains AWS connection credentials and communication parameters as described in [About Specifying the Configuration File Location](#s3_config_param).

Use the `section` parameter to specify the name of the configuration file section from which the `s3` protocol reads configuration parameters. The default `section` is named `default`. When you specify the section name in the configuration file, enclose it in brackets (for example, `[default]`).

## <a id="section_c2f_zvs_3x"></a>About Reading and Writing S3 Data Files 

You can use the `s3` protocol to read and write data files on Amazon S3.

**Reading S3 Files**

The S3 permissions on any file that you read must include `Open/Download` and `View` for the S3 user ID that accesses the files.

For read-only s3 tables, all of the files specified by the S3 file location \(S3\_endpoint/bucket\_name/S3\_prefix\) are used as the source for the external table and must have the same format. Each file must also contain complete data rows. If the files contain an optional header row, the column names in the header row cannot contain a newline character \(`\n`\) or a carriage return \(`\r`\). Also, the column delimiter cannot be a newline character \(`\n`\) or a carriage return character \(`\r`\).

The `s3` protocol recognizes gzip and deflate compressed files and automatically decompresses the files. For gzip compression, the protocol recognizes the format of a gzip compressed file. For deflate compression, the protocol assumes a file with the `.deflate` suffix is a deflate compressed file.

Each Greenplum Database segment can download one file at a time from the S3 location using several threads. To take advantage of the parallel processing performed by the Greenplum Database segments, the files in the S3 location should be similar in size and the number of files should allow for multiple segments to download the data from the S3 location. For example, if the Greenplum Database system consists of 16 segments and there was sufficient network bandwidth, creating 16 files in the S3 location allows each segment to download a file from the S3 location. In contrast, if the location contained only 1 or 2 files, only 1 or 2 segments download data.

**Writing S3 Files**

Writing a file to S3 requires that the S3 user ID have `Upload/Delete` permissions.

When you initiate an `INSERT` operation on a writable s3 table, each Greenplum Database segment uploads a single file to the configured S3 bucket using the filename format `<prefix><segment_id><random>.<extension>[.gz]` where:

-   `<prefix>` is the prefix specified in the S3 URL.
-   `<segment_id>` is the Greenplum Database segment ID.
-   `<random>` is a random number that is used to ensure that the filename is unique.
-   `<extension>` describes the file type \(`.txt` or .csv, depending on the value you provide in the `FORMAT` clause of `CREATE WRITABLE EXTERNAL TABLE`\). Files created by the `gpcheckcloud` utility always uses the extension .data.
-   .gz is appended to the filename if compression is enabled for s3 writable tables \(the default\).

You can configure the buffer size and the number of threads that segments use for uploading files. See [About the s3 Protocol Configuration File](#s3_config_file).

## <a id="s3_serversideencrypt"></a>s3 Protocol AWS Server-Side Encryption Support 

Greenplum Database supports server-side encryption using Amazon S3-managed keys \(SSE-S3\) for AWS S3 files you access with readable and writable external tables created using the `s3` protocol. SSE-S3 encrypts your object data as it writes to disk, and transparently decrypts the data for you when you access it.

**Note:** The `s3` protocol supports SSE-S3 only for Amazon Web Services S3 files. SS3-SE is not supported when accessing files in S3 compatible services.

Your S3 account permissions govern your access to all S3 bucket objects, whether the data is encrypted or not. However, you must configure your client to use S3-managed keys for accessing encrypted data.

Refer to [Protecting Data Using Server-Side Encryption](http://docs.aws.amazon.com/AmazonS3/latest/dev/serv-side-encryption.html) in the AWS documentation for additional information about AWS Server-Side Encryption.

**Configuring S3 Server-Side Encryption**

`s3` protocol server-side encryption is deactivated by default. To take advantage of server-side encryption on AWS S3 objects you write using the Greenplum Database `s3` protocol, you must set the `server_side_encryption` configuration parameter in your `s3` protocol configuration file to the value `sse-s3`:

```

server_side_encryption = sse-s3

```

When the configuration file you provide to a `CREATE WRITABLE EXTERNAL TABLE` call using the `s3` protocol includes the `server_side_encryption = sse-s3` setting, Greenplum Database applies encryption headers for you on all `INSERT` operations on that external table. S3 then encrypts on write the object\(s\) identified by the URI you provided in the `LOCATION` clause.

S3 transparently decrypts data during read operations of encrypted files accessed via readable external tables you create using the `s3` protocol. No additional configuration is required.

For further encryption configuration granularity, you may consider creating Amazon Web Services S3 *Bucket Policy*\(s\), identifying the objects you want to encrypt and the write actions on those objects as described in the [Protecting Data Using Server-Side Encryption with Amazon S3-Managed Encryption Keys \(SSE-S3\)](http://docs.aws.amazon.com/AmazonS3/latest/dev/UsingServerSideEncryption.html) AWS documentation.

## <a id="s3_proxy"></a>s3 Protocol Proxy Support 

You can specify a URL that is the proxy that S3 uses to connect to a data source. S3 supports these protocols: HTTP and HTTPS. You can specify a proxy with the `s3` protocol configuration parameter `proxy` or an environment variable. If the configuration parameter is set, the environment variables are ignored.

To specify proxy with an environment variable, you set the environment variable based on the protocol: `http_proxy` or `https_proxy`. You can specify a different URL for each protocol by setting the appropriate environment variable. S3 supports these environment variables.

-   `all_proxy` specifies the proxy URL that is used if an environment variable for a specific protocol is not set.
-   `no_proxy` specifies a comma-separated list of hosts names that do not use the proxy specified by an environment variable.

The environment variables must be set must and must be accessible to Greenplum Database on all Greenplum Database hosts.

For information about the configuration parameter `proxy`, see [About the s3 Protocol Configuration File](#s3_config_file).

## <a id="s3_auth"></a>About Providing the S3 Authentication Credentials

The `s3` protocol obtains the S3 authentication credentials as follows:

- You specify the S3 `accessid` and `secret` parameters and their values in a named `section` of an [s3 protocol configuration file](#s3_config_file). The default section from which the `s3` protocol obtains this information is named `[default]`.
- If you do not specify the `accessid` and `secret`, or these parameter values are empty, the `s3` protocol attempts to obtain the S3 authentication credentials from the `aws_access_key_id` and `aws_secret_access_key` parameters specified in a named `section` of the user's AWS credential file. The default location of this file is `~/.aws/credentials`, and the default section is named `[default]`.

## <a id="s3_config_file"></a>About the s3 Protocol Configuration File 

An `s3` protocol configuration file contains Amazon Web Services \(AWS\) connection credentials and communication parameters.

The `s3` protocol configuration file is a text file that contains named sections and parameters. The default section is named `[default]`. An example configuration file follows:

```
[default]
secret = "secret"
accessid = "user access id"
threadnum = 3
chunksize = 67108864
```

You can use the Greenplum Database `gpcheckcloud` utility to test the s3 protocol configuration file. See [Using the gpcheckcloud Utility](#s3chkcfg_utility).

**s3 Configuration File Parameters**

`accessid`
:   Optional. AWS S3 ID to access the S3 bucket. Refer to [About Providing the S3 Authentication Credentials](#s3_auth) for more information about specifying authentication credentials.

`secret`
:   Optional. AWS S3 passcode for the S3 ID to access the S3 bucket. Refer to [About Providing the S3 Authentication Credentials](#s3_auth) for more information about specifying authentication credentials.

`autocompress`
:   For writable s3 external tables, this parameter specifies whether to compress files \(using gzip\) before uploading to S3. Files are compressed by default if you do not specify this parameter.

`chunksize`
:   The buffer size that each segment thread uses for reading from or writing to the S3 server. The default is 64 MB. The minimum is 8MB and the maximum is 128MB.

When inserting data to a writable s3 table, each Greenplum Database segment writes the data into its buffer \(using multiple threads up to the `threadnum` value\) until it is full, after which it writes the buffer to a file in the S3 bucket. This process is then repeated as necessary on each segment until the insert operation completes.

Because Amazon S3 allows a maximum of 10,000 parts for multipart uploads, the minimum `chunksize` value of 8MB supports a maximum insert size of 80GB per Greenplum database segment. The maximum `chunksize` value of 128MB supports a maximum insert size 1.28TB per segment. For writable s3 tables, you must ensure that the `chunksize` setting can support the anticipated table size of your table. See [Multipart Upload Overview](http://docs.aws.amazon.com/AmazonS3/latest/dev/mpuoverview.html) in the S3 documentation for more information about uploads to S3.

`encryption`
:   Use connections that are secured with Secure Sockets Layer \(SSL\). Default value is `true`. The values `true`, `t`, `on`, `yes`, and `y` \(case insensitive\) are treated as `true`. Any other value is treated as `false`.

If the port is not specified in the URL in the `LOCATION` clause of the `CREATE EXTERNAL TABLE` command, the configuration file `encryption` parameter affects the port used by the `s3` protocol \(port 80 for HTTP or port 443 for HTTPS\). If the port is specified, that port is used regardless of the encryption setting.

`gpcheckcloud_newline`
:   When downloading files from an S3 location, the `gpcheckcloud` utility appends a new line character to last line of a file if the last line of a file does not have an EOL \(end of line\) character. The default character is `\n` \(newline\). The value can be `\n`, `\r` \(carriage return\), or `\n\r` \(newline/carriage return\).

Adding an EOL character prevents the last line of one file from being concatenated with the first line of next file.

`low_speed_limit`
:   The upload/download speed lower limit, in bytes per second. The default speed is 10240 \(10K\). If the upload or download speed is slower than the limit for longer than the time specified by `low_speed_time`, then the connection is stopped and retried. After 3 retries, the `s3` protocol returns an error. A value of 0 specifies no lower limit.

`low_speed_time`
:   When the connection speed is less than `low_speed_limit`, this parameter specified the amount of time, in seconds, to wait before cancelling an upload to or a download from the S3 bucket. The default is 60 seconds. A value of 0 specifies no time limit.

`proxy`
:   Specify a URL that is the proxy that S3 uses to connect to a data source. S3 supports these protocols: HTTP and HTTPS. This is the format for the parameter.

```
proxy = <protocol>://[<user>:<password>@]<proxyhost>[:<port>]
```

If this parameter is not set or is an empty string \(`proxy = ""`\), S3 uses the proxy specified by the environment variable `http_proxy` or `https_proxy` \(and the environment variables `all_proxy` and `no_proxy`\). The environment variable that S3 uses depends on the protocol. For information about the environment variables, see [s3 Protocol Proxy Support](g-s3-protocol.html#s3_proxy).

There can be at most one `proxy` parameter in the configuration file. The URL specified by the parameter is the proxy for all supported protocols.

`server_side_encryption`
:   The S3 server-side encryption method that has been configured for the bucket. Greenplum Database supports only server-side encryption with Amazon S3-managed keys, identified by the configuration parameter value `sse-s3`. Server-side encryption is deactivated \(`none`\) by default.

`threadnum`
:   The maximum number of concurrent threads a segment can create when uploading data to or downloading data from the S3 bucket. The default is 4. The minimum is 1 and the maximum is 8.

`verifycert`
:   Controls how the `s3` protocol handles authentication when establishing encrypted communication between a client and an S3 data source over HTTPS. The value is either `true` or `false`. The default value is `true`.

-   `verifycert=false` - Ignores authentication errors and allows encrypted communication over HTTPS.
-   `verifycert=true` - Requires valid authentication \(a proper certificate\) for encrypted communication over HTTPS.

Setting the value to `false` can be useful in testing and development environments to allow communication without changing certificates.

**Warning:** Setting the value to `false` exposes a security risk by ignoring invalid credentials when establishing communication between a client and a S3 data store.

`version`
:   Specifies the version of the information specified in the `LOCATION` clause of the `CREATE EXTERNAL TABLE` command. The value is either `1` or `2`. The default value is `1`.

If the value is `1`, the `LOCATION` clause supports an Amazon S3 URL, and does not contain the `region` parameter. If the value is `2`, the `LOCATION` clause supports S3 compatible services and must include the `region` parameter. The `region` parameter specifies the S3 data source region. For this S3 URL `s3://s3-us-west-2.amazonaws.com/s3test.example.com/dataset1/normal/`, the AWS S3 region is `us-west-2`.

If `version` is 1 or is not specified, this is an example of the `LOCATION` clause of the `CREATE EXTERNAL TABLE` command that specifies an Amazon S3 endpoint.

```
LOCATION ('s3://s3-us-west-2.amazonaws.com/s3test.example.com/dataset1/normal/ config=/home/gpadmin/aws_s3/s3.conf')
```

If `version` is 2, this is an example `LOCATION` clause with the `region` parameter for an AWS S3 compatible service.

```
LOCATION ('s3://test.company.com/s3test.company/test1/normal/ region=local-test config=/home/gpadmin/aws_s3/s3.conf') 
```

If `version` is 2, the `LOCATION` clause can also specify an Amazon S3 endpoint. This example specifies an Amazon S3 endpoint that uses the `region` parameter.

```
LOCATION ('s3://s3-us-west-2.amazonaws.com/s3test.example.com/dataset1/normal/ region=us-west-2 config=/home/gpadmin/aws_s3/s3.conf') 
```

**Note:** Greenplum Database can require up to `threadnum * chunksize` memory on each segment host when uploading or downloading S3 files. Consider this `s3` protocol memory requirement when you configure overall Greenplum Database memory.

## <a id="s3_config_param"></a>About Specifying the Configuration File Location 

The default location of the `s3` protocol configuration file is a file named `s3.conf` that resides in the data directory of each Greenplum Database segment instance:

```
<gpseg_data_dir>/<gpseg_prefix><N>/s3/s3.conf
```

The gpseg\_data\_dir is the path to the Greenplum Database segment data directory, the gpseg\_prefix is the segment prefix, and N is the segment ID. The segment data directory, prefix, and ID are set when you initialize a Greenplum Database system.

You may choose an alternate location for the `s3` protocol configuration file by specifying the optional `config` or `config_server` parameters in the `LOCATION` URL:

-   You can simplify the configuration by using a single configuration file that resides in the same file system location on each segment host. In this scenario, you specify the `config` parameter in the `LOCATION` clause to identify the absolute path to the file. The following example specifies a location in the `gpadmin` home directory:

    ```
    LOCATION ('s3://s3-us-west-2.amazonaws.com/test/my_data config=/home/gpadmin/s3.conf')
    ```

    The `/home/gpadmin/s3.conf` file must reside on each segment host, and all segment instances on a host use the file.

-   You also have the option to use an `http/https` server to serve up the configuration file. In this scenario, you specify an `http/https` server URL in the `config_server` parameter. You are responsible for configuring and starting the server, and each Greenplum Database segment host must be able to access the server. The following example specifies an IP address and port for an `https` server:

    ```
    LOCATION ('s3://s3-us-west-2.amazonaws.com/test/my_data config_server=https://203.0.113.0:8553')
    ```


## <a id="section_tsq_n3t_3x"></a>s3 Protocol Limitations 

These are `s3` protocol limitations:

-   Only the S3 path-style URL is supported.

    ```
    s3://<S3_endpoint>/<bucketname>/[<S3_prefix>]
    ```

-   Only the S3 endpoint is supported. The protocol does not support virtual hosting of S3 buckets \(binding a domain name to an S3 bucket\).
-   AWS signature version 4 signing process is supported.

    For information about the S3 endpoints supported by each signing process, see [http://docs.aws.amazon.com/general/latest/gr/rande.html\#s3\_region](http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region).

-   Only a single URL and optional configuration file location and region parameters is supported in the `LOCATION` clause of the `CREATE EXTERNAL TABLE` command.
-   If the `NEWLINE` parameter is not specified in the `CREATE EXTERNAL TABLE` command, the newline character must be identical in all data files for specific prefix. If the newline character is different in some data files with the same prefix, read operations on the files might fail.
-   For writable s3 external tables, only the `INSERT` operation is supported. `UPDATE`, `DELETE`, and `TRUNCATE` operations are not supported.
-   Because Amazon S3 allows a maximum of 10,000 parts for multipart uploads, the maximum `chunksize` value of 128MB supports a maximum insert size of 1.28TB per Greenplum database segment for writable s3 tables. You must ensure that the `chunksize` setting can support the anticipated table size of your table. See [Multipart Upload Overview](http://docs.aws.amazon.com/AmazonS3/latest/dev/mpuoverview.html) in the S3 documentation for more information about uploads to S3.
-   To take advantage of the parallel processing performed by the Greenplum Database segment instances, the files in the S3 location for read-only s3 tables should be similar in size and the number of files should allow for multiple segments to download the data from the S3 location. For example, if the Greenplum Database system consists of 16 segments and there was sufficient network bandwidth, creating 16 files in the S3 location allows each segment to download a file from the S3 location. In contrast, if the location contained only 1 or 2 files, only 1 or 2 segments download data.

## <a id="s3chkcfg_utility"></a>Using the gpcheckcloud Utility 

The Greenplum Database utility `gpcheckcloud` helps users create an `s3` protocol configuration file and test a configuration file. You can specify options to test the ability to access an S3 bucket with a configuration file, and optionally upload data to or download data from files in the bucket.

If you run the utility without any options, it sends a template configuration file to `STDOUT`. You can capture the output and create an `s3` configuration file to connect to Amazon S3.

The utility is installed in the Greenplum Database `$GPHOME/bin` directory.

**Syntax**

```
gpcheckcloud {-c | -d} "s3://<S3_endpoint>/<bucketname>/[<S3_prefix>] [config=<path_to_config_file>]"

gpcheckcloud -u <file_to_upload> "s3://<S3_endpoint>/<bucketname>/[<S3_prefix>] [config=<path_to_config_file>]"
gpcheckcloud -t

gpcheckcloud -h
```

**Options**

`-c`
:   Connect to the specified S3 location with the configuration specified in the `s3` protocol URL and return information about the files in the S3 location.

If the connection fails, the utility displays information about failures such as invalid credentials, prefix, or server address \(DNS error\), or server not available.

`-d`
:   Download data from the specified S3 location with the configuration specified in the `s3` protocol URL and send the output to `STDOUT`.

If files are gzip compressed or have a `.deflate` suffix to indicate deflate compression, the uncompressed data is sent to `STDOUT`.

`-u`
:   Upload a file to the S3 bucket specified in the `s3` protocol URL using the specified configuration file if available. Use this option to test compression and `chunksize` and `autocompress` settings for your configuration.

`-t`
:   Sends a template configuration file to `STDOUT`. You can capture the output and create an `s3` configuration file to connect to Amazon S3.

`-h`
:   Display `gpcheckcloud` help.

**Examples**

This example runs the utility without options to create a template `s3` configuration file `mytest_s3.config` in the current directory.

```
gpcheckcloud -t > ./mytest_s3.config
```

This example attempts to upload a local file, test-data.csv to an S3 bucket location using the `s3` configuration file `s3.mytestconf`:

```
gpcheckcloud -u ./test-data.csv "s3://s3-us-west-2.amazonaws.com/test1/abc config=s3.mytestconf"
```

A successful upload results in one or more files placed in the S3 bucket using the filename format `abc<segment_id><random>.data[.gz]`. See [About Reading and Writing S3 Data Files](#section_c2f_zvs_3x).

This example attempts to connect to an S3 bucket location with the `s3` protocol configuration file `s3.mytestconf`.

```
gpcheckcloud -c "s3://s3-us-west-2.amazonaws.com/test1/abc config=s3.mytestconf"
```

This example attempts to connect to an S3 bucket location using the default location for the `s3` protocol configuration file \(`s3/s3.conf` in segment data directories\):

```
gpcheckcloud -c "s3://s3-us-west-2.amazonaws.com/test1/abc"
```

Download all files from the S3 bucket location and send the output to `STDOUT`.

```
gpcheckcloud -d "s3://s3-us-west-2.amazonaws.com/test1/abc config=s3.mytestconf"
```

**Parent topic:** [Defining External Tables](../external/g-external-tables.html)

