# Using the S3 Storage Plugin with gpbackup and gprestore 

The S3 storage plugin application lets you use an Amazon Simple Storage Service \(Amazon S3\) location to store and retrieve backups when you run [gpbackup](../../utility_guide/admin_utilities/gpbackup.html) and [gprestore](../../utility_guide/admin_utilities/gprestore.html). Amazon S3 provides secure, durable, highly-scalable object storage.

The S3 storage plugin can also connect to an Amazon S3 compatible service such as [Dell EMC Elastic Cloud Storage](https://www.emc.com/en-us/storage/ecs/index.htm) \(ECS\) and [Minio](https://www.minio.io/).

To use the S3 storage plugin application, you specify the location of the plugin and the S3 login and backup location in a configuration file. When you run `gpbackup` or `gprestore`, you specify the configuration file with the option `--plugin-config`. For information about the configuration file, see [S3 Storage Plugin Configuration File Format](#s3-plugin-config).

If you perform a backup operation with the `gpbackup` option `--plugin-config`, you must also specify the `--plugin-config` option when you restore the backup with `gprestore`.

## S3 Storage Plugin Configuration File Format 

The configuration file specifies the absolute path to the Greenplum Database S3 storage plugin executable, connection credentials, and S3 location.

The S3 storage plugin configuration file uses the [YAML 1.1](http://yaml.org/spec/1.1/) document format and implements its own schema for specifying the location of the Greenplum Database S3 storage plugin, connection credentials, and S3 location and login information.

The configuration file must be a valid YAML document. The `gpbackup` and `gprestore` utilities process the control file document in order and use indentation \(spaces\) to determine the document hierarchy and the relationships of the sections to one another. The use of white space is significant. White space should not be used simply for formatting purposes, and tabs should not be used at all.

This is the structure of a S3 storage plugin configuration file.

```
[executablepath](#s3-exe-path): <<absolute-path-to-gpbackup_s3_plugin>>
[options](#s3-options): 
  [region](#s3-region): <<aws-region>>
  [endpoint](#s3-endpoint): <<S3-endpoint>>
  [aws\_access\_key\_id](#s3-id): <<aws-user-id>>
  [aws\_secret\_access\_key](#s3-key): <<aws-user-id-key>>
  [bucket](#s3-bucket): <<s3-bucket>>
  [folder](#s3-location): <<s3-location>>
  [encryption](#s3-encryption): [on|off]
```

executablepath
:   Required. Absolute path to the plugin executable. For example, the Tanzu Greenplum installation location is `$GPHOME/bin/gpbackup_s3_plugin`. The plugin must be in the same location on every Greenplum Database host.

options
:   Required. Begins the S3 storage plugin options section.

    region
    :   Required for AWS S3. If connecting to an S3 compatible service, this option is not required.

    endpoint
    :   Required for an S3 compatible service. Specify this option to connect to an S3 compatible service such as ECS. The plugin connects to the specified S3 endpoint \(hostname or IP address\) to access the S3 compatible data store.

    :   If this option is specified, the plugin ignores the `region` option and does not use AWS to resolve the endpoint. When this option is not specified, the plugin uses the `region` to determine AWS S3 endpoint.

    aws\_access\_key\_id
    :   Optional. The S3 ID to access the S3 bucket location that stores backup files.

    :   If this parameter is not specified, S3 authentication information from the session environment is used. See [Notes](#s3_notes).

    aws\_secret\_access\_key
    :   Required only if you specify `aws_access_key_id`. The S3 passcode for the S3 ID to access the S3 bucket location.

    bucket
    :   Required. The name of the S3 bucket in the AWS region or S3 compatible data store. The bucket must exist.

    folder
    :   Required. The S3 location for backups. During a backup operation, the plugin creates the S3 location if it does not exist in the S3 bucket.

    encryption
    :   Optional. Enable or disable use of Secure Sockets Layer \(SSL\) when connecting to an S3 location. Default value is `on`, use connections that are secured with SSL. Set this option to `off` to connect to an S3 compatible service that is not configured to use SSL.

    :   Any value other than `off` is treated as `on`.

## Example 

This is an example S3 storage plugin configuration file that is used in the next `gpbackup` example command. The name of the file is `s3-test-config.yaml`.

```
executablepath: $GPHOME/bin/gpbackup_s3_plugin
options: 
  region: us-west-2
  aws_access_key_id: test-s3-user
  aws_secret_access_key: asdf1234asdf
  bucket: gpdb-backup
  folder: test/backup3
```

This `gpbackup` example backs up the database demo using the S3 storage plugin. The absolute path to the S3 storage plugin configuration file is `/home/gpadmin/s3-test`.

```
gpbackup --dbname demo --plugin-config /home/gpadmin/s3-test-config.yaml
```

The S3 storage plugin writes the backup files to this S3 location in the AWS region us-west-2.

```
gpdb-backup/test/backup3/backups/<YYYYMMDD>/<YYYYMMDDHHMMSS>/
```

## Notes 

The S3 storage plugin application must be in the same location on every Greenplum Database host. The configuration file is required only on the master host.

When you perform a backup with the S3 storage plugin, the plugin stores the backup files in this location in the S3 bucket.

```
<<folder>>/backups/<<datestamp>>/<<timestamp>>
```

Where folder is the location you specified in the S3 configuration file, and datestamp and timestamp are the backup date and time stamps.

Using Amazon S3 to back up and restore data requires an Amazon AWS account with access to the Amazon S3 bucket. These are the Amazon S3 bucket permissions required for backing up and restoring data.

-   Upload/Delete for the S3 user ID that uploads the files
-   Open/Download and View for the S3 user ID that accesses the files

If `aws_access_key_id` and `aws_secret_access_key` are not specified in the configuration file, the S3 plugin uses S3 authentication information from the system environment of the session running the backup operation. The S3 plugin searches for the information in these sources, using the first available source.

1.  The environment variables `AWS_ACCESS_KEY_ID` and `AWS_SECRET_ACCESS_KEY`.
2.  The authentication information set with the AWS CLI command `aws configure`.
3.  The credentials of the Amazon EC2 IAM role if the backup is run from an EC2 instance.

For information about Amazon S3, see [Amazon S3](https://aws.amazon.com/s3/).

-   For information about Amazon S3 regions and endpoints, see [http://docs.aws.amazon.com/general/latest/gr/rande.html\#s3\_region](http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region).
-   For information about S3 buckets and folders, see the Amazon S3 documentation [https://aws.amazon.com/documentation/s3/](https://aws.amazon.com/documentation/s3/).

**Parent topic:** [Using gpbackup Storage Plugins](../managing/backup-plugins.html)

