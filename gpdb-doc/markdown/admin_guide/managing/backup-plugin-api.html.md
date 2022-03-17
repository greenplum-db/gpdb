---
title: Backup/Restore Storage Plugin API 
---

This topic describes how to develop a custom storage plugin with the Greenplum Database Backup/Restore Storage Plugin API.

The Backup/Restore Storage Plugin API provides a framework that you can use to develop and integrate a custom backup storage system with the Greenplum Database [`gpbackup`](../../utility_guide/ref/gpbackup.html), [`gpbackup_manager`](../../utility_guide/ref/gpbackup_manager.html), and [`gprestore`](../../utility_guide/ref/gprestore.html) utilities.

The Backup/Restore Storage Plugin API defines a set of interfaces that a plugin must support. The API also specifies the format and content of a configuration file for a plugin.

When you use the Backup/Restore Storage Plugin API, you create a plugin that the Greenplum Database administrator deploys to the Greenplum Database cluster. Once deployed, the plugin is available for use in certain backup and restore operations.

This topic includes the following subtopics:

-   [Plugin Configuration File](#topic9339)
-   [Plugin API](#topic_api)
-   [Plugin Commands](#topic_commands)
-   [Implementing a Backup/Restore Storage Plugin](#topic9337)
-   [Verifying a Backup/Restore Storage Plugin](#topic9339a)
-   [Packaging and Deploying a Backup/Restore Storage Plugin](#topic9339b)

**Parent topic:**[Parallel Backup with gpbackup and gprestore](../managing/backup-gpbackup.html)

## <a id="topic9339"></a>Plugin Configuration File 

Specifying the `--plugin-config` option to the `gpbackup` and `gprestore` commands instructs the utilities to use the plugin specified in the configuration file for the operation.

The plugin configuration file provides information for both Greenplum Database and the plugin. The Backup/Restore Storage Plugin API defines the format of, and certain keywords used in, the plugin configuration file.

A plugin configuration file is a YAML file in the following format:

```
executablepath: <path_to_plugin_executable>
options:
  <keyword1>: <value1>
  <keyword2>: <value2>
  ...
  <keywordN>: <valueN>
```

`gpbackup` and `gprestore` use the `**executablepath**` value to determine the file system location of the plugin executable program.

The plugin configuration file may also include keywords and values specific to a plugin instance. A backup/restore storage plugin can use the `**options**` block specified in the file to obtain information from the user that may be required to perform its tasks. This information may include location, connection, or authentication information, for example. The plugin should both specify and consume the content of this information in `keyword:value` syntax.

A sample plugin configuration file for the Greenplum Database S3 backup/restore storage plugin follows:

```
executablepath: $GPHOME/bin/gpbackup_s3_plugin
options:
  region: us-west-2
  aws_access_key_id: notarealID
  aws_secret_access_key: notarealkey
  bucket: gp_backup_bucket
  folder: greenplum_backups
```

## <a id="topic_api"></a>Plugin API 

The plugin that you implement when you use the Backup/Restore Storage Plugin API is an executable program that supports specific commands invoked by `gpbackup` and `gprestore` at defined points in their respective life cycle operations:

-   The Greenplum Database Backup/Restore Storage Plugin API provides hooks into the `gpbackup` lifecycle at initialization, during backup, and at cleanup/exit time.
-   The API provides hooks into the `gprestore` lifecycle at initialization, during restore, and at cleanup/exit time.
-   The API provides arguments that specify the execution scope \(master host, segment host, or segment instance\) for a plugin setup or cleanup command. The scope can be one of these values.

    -   `master` - Run the plugin once on the master host.
    -   `segment_host` - Run the plugin once on each of the segment hosts.
    -   `segment` - Run the plugin once for each active segment instance on the host running the segment instance.
    The Greenplum Database hosts and segment instances are based on the Greenplum Database configuration when the back up started. The values `segment_host` and `segment` are provided as a segment host can host multiple segment instances. There might be some setup or cleanup required at the segment host level as compared to each segment instance.


The Plugin API also defines the `delete_backup` command, which is called by the `gpbackup_manager` utility. \(The `gpbackup_manager` source code is proprietary and the utility is available only in the Tanzu Greenplum Backup and Restore download from [VMware Tanzu Network](https://network.pivotal.io).\)

The Backup/Restore Storage Plugin API defines the following call syntax for a backup/restore storage plugin executable program:

```
plugin_executable command config_file args
```

where:

-   `plugin_executable` - The absolute path of the backup/restore storage plugin executable program. This path is determined by the `executablepath` property value configured in the plugin's configuration YAML file.
-   `command` - The name of a Backup/Restore Storage Plugin API command that identifies a specific entry point to a `gpbackup` or `gprestore` lifecycle operation.
-   `config_file` - The absolute path of the plugin's configuration YAML file.
-   `args` - The command arguments; the actual arguments differ depending upon the `command` specified.

### <a id="topic_commands"></a>Plugin Commands 

The Greenplum Database Backup/Restore Storage Plugin API defines the following commands:

|Command Name|Description|
|------------|-----------|
|[plugin\_api\_version](plugin_api/plugin_api_version.html)|Return the version of the Backup/Restore Storage Plugin API supported by the plugin. The currently supported version is 0.4.0.|
|[setup\_plugin\_for\_backup](plugin_api/setup_plugin_for_backup.html)|Initialize the plugin for a backup operation.|
|[backup\_file](plugin_api/backup_file.html)|Move a backup file to the remote storage system.|
|[backup\_data](plugin_api/backup_data.html)|Move streaming data from `stdin` to a file on the remote storage system.|
|[delete\_backup](plugin_api/delete_backup.html)|Delete the directory specified by the given backup timestamp on the remote system.|
|[cleanup\_plugin\_for\_backup](plugin_api/cleanup_plugin_for_backup.html)|Clean up after a backup operation.|
|[setup\_plugin\_for\_restore](plugin_api/setup_plugin_for_restore.html)|Initialize the plugin for a restore operation.|
|[restore\_file](plugin_api/restore_file.html)|Move a backup file from the remote storage system to a designated location on the local host.|
|[restore\_data](plugin_api/restore_data.html)|Move a backup file from the remote storage system, streaming the data to `stdout`.|
|[cleanup\_plugin\_for\_restore](plugin_api/cleanup_plugin_for_restore.html)|Clean up after a restore operation.|

A backup/restore storage plugin must support every command identified above, even if it is a no-op.

## <a id="topic9337"></a>Implementing a Backup/Restore Storage Plugin 

You can implement a backup/restore storage plugin executable in any programming or scripting language.

The tasks performed by a backup/restore storage plugin will be very specific to the remote storage system. As you design the plugin implementation, you will want to:

-   Examine the connection and data transfer interface to the remote storage system.
-   Identify the storage path specifics of the remote system.
-   Identify configuration information required from the user.
-   Define the keywords and value syntax for information required in the plugin configuration file.
-   Determine if, and how, the plugin will modify \(compress, etc.\) the data en route to/from the remote storage system.
-   Define a mapping between a `gpbackup` file path and the remote storage system.
-   Identify how `gpbackup` options affect the plugin, as well as which are required and/or not applicable. For example, if the plugin performs its own compression, `gpbackup` must be invoked with the `--no-compression` option to prevent the utility from compressing the data.

A backup/restore storage plugin that you implement must:

-   Support all plugin commands identified in [Plugin Commands](#topic_commands). Each command must exit with the values identified on the command reference page.

Refer to the [gpbackup-s3-plugin](https://github.com/greenplum-db/gpbackup-s3-plugin) github repository for an example plugin implementation.

## <a id="topic9339a"></a>Verifying a Backup/Restore Storage Plugin 

The Backup/Restore Storage Plugin API includes a test bench that you can run to ensure that a plugin is well integrated with `gpbackup` and `gprestore`.

The test bench is a `bash` script that you run in a Greenplum Database installation. The script generates a small \(<1MB\) data set in a Greenplum Database table, explicitly tests each command, and runs a backup and restore of the data \(file and streaming\). The test bench invokes `gpbackup` and `gprestore`, which in turn individually call/test each Backup/Restore Storage Plugin API command implemented in the plugin.

The test bench program calling syntax is:

```
plugin_test_bench.sh <plugin_executable plugin_config>
```

### <a id="topic8339191"></a>Procedure 

To run the Backup/Restore Storage Plugin API test bench against a plugin:

1.  Log in to the Greenplum Database master host and set up your environment. For example:

    ```
    $ ssh gpadmin@<gpmaster>
    gpadmin@gpmaster$ . /usr/local/greenplum-db/greenplum_path.sh
    ```

2.  Obtain a copy of the test bench from the `gpbackup` github repository. For example:

    ```
    $ git clone git@github.com:greenplum-db/gpbackup.git
    ```

    The clone operation creates a directory named `gpbackup/` in the current working directory.

3.  Locate the test bench program in the `gpbackup/master/plugins` directory. For example:

    ```
    $ ls gpbackup/master/plugins/plugin_test_bench.sh
    ```

4.  Copy the plugin executable program and the plugin configuration YAML file from your development system to the Greenplum Database master host. Note the file system location to which you copied the files.
5.  Copy the plugin executable program from the Greenplum Database master host to the same file system location on each segment host.
6.  If required, edit the plugin configuration YAML file to specify the absolute path of the plugin executable program that you just copied to the Greenplum segments.
7.  Run the test bench program against the plugin. For example:

    ```
    $ gpbackup/master/plugins/plugin_test_bench.sh /path/to/pluginexec /path/to/plugincfg.yaml
    ```

8.  Examine the test bench output. Your plugin passed the test bench if all output messages specify `RUNNING` and `PASSED`. For example:

    ```
    # ----------------------------------------------
    # Starting gpbackup plugin tests
    # ----------------------------------------------
    [RUNNING] plugin_api_version
    [PASSED] plugin_api_version
    [RUNNING] setup_plugin_for_backup
    [RUNNING] backup_file
    [RUNNING] setup_plugin_for_restore
    [RUNNING] restore_file
    [PASSED] setup_plugin_for_backup
    [PASSED] backup_file
    [PASSED] setup_plugin_for_restore
    [PASSED] restore_file
    [RUNNING] backup_data
    [RUNNING] restore_data
    [PASSED] backup_data
    [PASSED] restore_data
    [RUNNING] cleanup_plugin_for_backup
    [PASSED] cleanup_plugin_for_backup
    [RUNNING] cleanup_plugin_for_restore
    [PASSED] cleanup_plugin_for_restore
    [RUNNING] gpbackup with test database
    [RUNNING] gprestore with test database
    [PASSED] gpbackup and gprestore
    # ----------------------------------------------
    # Finished gpbackup plugin tests
    # ----------------------------------------------
    ```


## <a id="topic9339b"></a>Packaging and Deploying a Backup/Restore Storage Plugin 

Your backup/restore storage plugin is ready to be deployed to a Greenplum Database installation after the plugin passes your testing and the test bench verification. When you package the backup/restore storage plugin, consider the following:

-   The backup/restore storage plugin must be installed in the same file system location on every host in the Greenplum Database cluster. Provide installation instructions for the plugin identifying the same.
-   The `gpadmin` user must have permission to traverse the file system path to the backup/restore plugin executable program.
-   Include a template configuration file with the plugin.
-   Document the valid plugin configuration keywords, making sure to include the syntax of expected values.
-   Document required `gpbackup` options and how they affect plugin processing.

