---
title: Using the S3 Storage Plugin with gpbackup and gprestore 
---

The S3 storage plugin application lets you use an Amazon Simple Storage Service \(Amazon S3\) location to store and retrieve backups when you run [gpbackup](../../utility_guide/ref/gpbackup.html) and [gprestore](../../utility_guide/ref/gprestore.html). Amazon S3 provides secure, durable, highly-scalable object storage. The S3 plugin streams the backup data from a named pipe \(FIFO\) directly to the S3 bucket without generating local disk I/O.

The S3 storage plugin can also connect to an Amazon S3 compatible service such as [Dell EMC Elastic Cloud Storage](https://www.emc.com/en-us/storage/ecs/index.htm) \(ECS\), [Minio](https://www.minio.io/), and [Cloudian HyperStore](https://cloudian.com/products/hyperstore/).

## <a id="prereq"></a>Prerequisites 

Using Amazon S3 to back up and restore data requires an Amazon AWS account with access to the Amazon S3 bucket. These are the Amazon S3 bucket permissions required for backing up and restoring data:

-   Upload/Delete for the S3 user ID that uploads the files
-   Open/Download and View for the S3 user ID that accesses the files

For information about Amazon S3, see [Amazon S3](https://aws.amazon.com/s3/). For information about Amazon S3 regions and endpoints, see [AWS service endpoints](http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region). For information about S3 buckets and folders, see the [Amazon S3 documentation](https://aws.amazon.com/documentation/s3/).

## <a id="insts3"></a>Installing the S3 Storage Plugin 

The S3 storage plugin is included with the Tanzu Greenplum Backup and Restore release. Use the latest S3 plugin release with the latest Tanzu Backup and Restore, to avoid any incompatibilities.

Open Source Greenplum Backup and Restore customers may get the utility from [gpbackup-s3-plugin](https://github.com/greenplum-db/gpbackup-s3-plugin). Build the utility following the steps in [Building and Installing the S3 plugin](https://github.com/greenplum-db/gpbackup-s3-plugin#building-and-installing-binaries).

The S3 storage plugin application must be in the same location on every Greenplum Database host, for example `$GPHOME/bin/gpbackup_s3_plugin`. The S3 storage plugin requires a configuration file, installed only on the master host.

## <a id="uses3"></a>Using the S3 Storage Plugin 

To use the S3 storage plugin application, specify the location of the plugin, the S3 login credentials, and the backup location in a configuration file. For information about the configuration file, see [S3 Storage Plugin Configuration File Format](#s3-plugin-config).

When running `gpbackup` or `gprestore`, specify the configuration file with the option `--plugin-config`.

```
gpbackup --dbname <database-name> --plugin-config /<path-to-config-file>/<s3-config-file>.yaml
```

When you perform a backup operation using `gpbackup` with the `--plugin-config` option, you must also specify the `--plugin-config` option when restoring with `gprestore`.

```
gprestore --timestamp <YYYYMMDDHHMMSS> --plugin-config /<path-to-config-file>/<s3-config-file>.yaml
```

The S3 plugin stores the backup files in the S3 bucket, in a location similar to:

```
<folder>/backups/<datestamp>/<timestamp>
```

Where folder is the location you specified in the S3 configuration file, and datestamp and timestamp are the backup date and time stamps.

The S3 storage plugin logs are in `<gpadmin_home>/gpAdmin/gpbackup_s3_plugin_timestamp.log` on each Greenplum host system. The timestamp format is `YYYYMMDDHHMMSS`.

**Example**

This is an example S3 storage plugin configuration file, named `s3-test-config.yaml`, that is used in the next `gpbackup` example command.

```
executablepath: $GPHOME/bin/gpbackup_s3_plugin
options: 
  region: us-west-2
  aws_access_key_id: test-s3-user
  aws_secret_access_key: asdf1234asdf
  bucket: gpdb-backup
  folder: test/backup3
```

This `gpbackup` example backs up the database demo using the S3 storage plugin with absolute path `/home/gpadmin/s3-test`.

```
gpbackup --dbname demo --plugin-config /home/gpadmin/s3-test/s3-test-config.yaml
```

The S3 storage plugin writes the backup files to this S3 location in the AWS region us-west-2.

```
gpdb-backup/test/backup3/backups/<YYYYMMDD>/<YYYYMMDDHHMMSS>/
```

This example restores a specific backup set defined by the `20201206233124` timestamp, using the S3 plugin configuration file.

```
gprestore --timestamp 20201206233124 --plugin-config /home/gpadmin/s3-test/s3-test-config.yaml
```

## <a id="s3-plugin-config"></a>S3 Storage Plugin Configuration File Format 

The configuration file specifies the absolute path to the Greenplum Database S3 storage plugin executable, connection credentials, and S3 location.

The S3 storage plugin configuration file uses the [YAML 1.1](http://yaml.org/spec/1.1/) document format and implements its own schema for specifying the location of the Greenplum Database S3 storage plugin, connection credentials, and S3 location and login information.

The configuration file must be a valid YAML document. The `gpbackup` and `gprestore` utilities process the control file document in order and use indentation \(spaces\) to determine the document hierarchy and the relationships of the sections to one another. The use of white space is significant. White space should not be used simply for formatting purposes, and tabs should not be used at all.

This is the structure of a S3 storage plugin configuration file.

```
executablepath: <absolute-path-to-gpbackup_s3_plugin>
options: 
  region: <aws-region>
  endpoint: <S3-endpoint>
  aws_access_key_id: <aws-user-id>
  aws_secret_access_key: <aws-user-id-key>
  bucket: <s3-bucket>
  folder: <s3-location>
  encryption: [on|off]
  backup_max_concurrent_requests: [int]
    # default value is 6
  backup_multipart_chunksize: [string] 
    # default value is 500MB
  restore_max_concurrent_requests: [int]
    # default value is 6
  restore_multipart_chunksize: [string] 
    # default value is 500MB
  http_proxy:
        http://<your_username>:<your_secure_password>@proxy.example.com:proxy_port
```

**Note:** The S3 storage plugin does not support filtered restore operations and the associated `restore_subset` plugin configuration property.

`executablepath`
:   Required. Absolute path to the plugin executable. For example, the Tanzu Greenplum installation location is `$GPHOME/bin/gpbackup_s3_plugin`. The plugin must be in the same location on every Greenplum Database host.

`options`
:   Required. Begins the S3 storage plugin options section.

`region`
:   Required for AWS S3. If connecting to an S3 compatible service, this option is not required, **with one exception**: If you are using Minio object storage and have specified a value for the `Region` setting on the Minio server side you must set this `region` option to the same value.

`endpoint`
:   Required for an S3 compatible service. Specify this option to connect to an S3 compatible service such as ECS. The plugin connects to the specified S3 endpoint \(hostname or IP address\) to access the S3 compatible data store.

If this option is specified, the plugin ignores the `region` option and does not use AWS to resolve the endpoint. When this option is not specified, the plugin uses the `region` to determine AWS S3 endpoint.

`aws_access_key_id`
:   Optional. The S3 ID to access the S3 bucket location that stores backup files.

If this parameter is not specified, S3 authentication uses information from the session environment. See [aws\_secret\_access\_key](#s3-key)

`aws_secret_access_key`
:   Required only if you specify `aws_access_key_id`. The S3 passcode for the S3 ID to access the S3 bucket location.

If `aws_access_key_id` and `aws_secret_access_key` are not specified in the configuration file, the S3 plugin uses S3 authentication information from the system environment of the session running the backup operation. The S3 plugin searches for the information in these sources, using the first available source.

1.  The environment variables `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`.
2.  The authentication information set with the AWS CLI command `aws configure`.
3.  The credentials of the Amazon EC2 IAM role if the backup is run from an EC2 instance.

`bucket`
:   Required. The name of the S3 bucket in the AWS region or S3 compatible data store. The bucket must exist.

`folder`
:   Required. The S3 location for backups. During a backup operation, the plugin creates the S3 location if it does not exist in the S3 bucket.

`encryption`
:   Optional. Enable or disable use of Secure Sockets Layer \(SSL\) when connecting to an S3 location. Default value is `on`, use connections that are secured with SSL. Set this option to `off` to connect to an S3 compatible service that is not configured to use SSL.

Any value other than `on` or `off` is not accepted.

`backup_max_concurrent_requests`
:   Optional. The segment concurrency level for a file artifact within a single backup/upload request. The default value is set to 6. Use this parameter in conjuction with the `gpbackup --jobs` flag, to increase your overall backup concurrency.

Example: In a 4 node cluster, with 12 segments \(3 per node\), if the `--jobs` flag is set to 10, there could be 120 concurrent backup requests. With the `backup_max_concurrent_requests` parameter set to 6, the total S3 concurrent upload threads during a single backup session would reach 720 \(120 x 6\).

**Note:** If the upload artifact is 10MB \(see [backup\_multipart\_chunksize](#multipart_chunksize)\), the `backup_max_concurrent_requests` parameter would not take effect since the file is smaller than the chunk size.

`backup_multipart_chunksize`
:   Optional. The file chunk size of the S3 multipart upload request in Megabytes \(for example 20MB\), Gigabytes \(for example 1GB\), or bytes \(for example 1048576B\). The default value is 500MB and the minimum value is 5MB \(or 5242880B\). Use this parameter along with the `--jobs`flag and the `backup_max_concurrent_requests` parameter to fine tune your backups. Set the chunksize based on your individual segment file size. S3 supports up to 10,000 max total partitions for a single file upload.

`restore_max_concurrent_requests`
:   Optional. The level of concurrency for downloading a file artifact within a single restore request. The default value is set to 6.

`restore_multipart_chunksize`
:   Optional. The file chunk size of the S3 multipart download request in Megabytes \(for example 20MB\), Gigabytes \(for example 1GB\), or bytes \(for example 1048576B\). The default value is 500MB. Use this parameter along with the `restore_max_concurrent_requests` parameter to fine tune your restores.

`http_proxy`
:   Optional. Allow AWS S3 access via a proxy server. The parameter should contain the proxy url in the form of `http://username:password@proxy.example.com:proxy_port` or `http://proxy.example.com:proxy_port`.

**Parent topic:**[Using gpbackup Storage Plugins](../managing/backup-plugins.html)

