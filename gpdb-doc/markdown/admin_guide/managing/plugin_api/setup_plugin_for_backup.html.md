---
title: setup\_plugin\_for\_backup 
---

Plugin command to initialize a storage plugin for the backup operation.

## <a id="section2"></a>Synopsis 

```
<plugin_executable> setup_plugin_for_backup <plugin_config_file> <local_backup_dir> <scope>
```

```
<plugin_executable> setup_plugin_for_backup <plugin_config_file> <local_backup_dir> <scope> <contentID>
```

## <a id="section3"></a>Description 

`gpbackup` invokes the `setup_plugin_for_backup` plugin command during `gpbackup` initialization phase. The scope argument specifies the execution scope. `gpbackup` will invoke the command with each of the scope values.

The `setup_plugin_for_backup` command should perform the activities necessary to initialize the remote storage system before backup begins. Set up activities may include creating remote directories, validating connectivity to the remote storage system, checking disks, and so forth.

## <a id="section4"></a>Arguments 

plugin\_config\_file
:   The absolute path to the plugin configuration YAML file.

local\_backup\_dir
:   The local directory on the Greenplum Database host \(master and segments\) to which `gpbackup` will write backup files. `gpbackup` creates this local directory.

    -   When scope is `master`, the local\_backup\_dir is the backup directory of the Greenplum Database master.
    -   When scope is `segment`, the local\_backup\_dir is the backup directory of a segment instance. The contentID identifies the segment instance.
    -   When the scope is `segment_host`, the local\_backup\_dir is an arbitrary backup directory on the host.

scope
:   The execution scope value indicates the host and number of times the plugin command is run. scope can be one of these values:

    -   `master` - Run the plugin command once on the master host.
    -   `segment_host` - Run the plugin command once on each of the segment hosts.
    -   `segment` - Run the plugin command once for each active segment instance on the host running the segment instance. The contentID identifies the segment instance.

:   The Greenplum Database hosts and segment instances are based on the Greenplum Database configuration when the back up was first initiated.

contentID
:   The contentID of the Greenplum Database master or segment instance corresponding to the scope. contentID is passed only when the scope is `master` or `segment`.

    -   When scope is `master`, the contentID is `-1`.
    -   When scope is `segment`, the contentID is the content identifier of an active segment instance.

## <a id="section5"></a>Exit Code 

The `setup_plugin_for_backup` command must exit with a value of 0 on success, non-zero if an error occurs. In the case of a non-zero exit code, `gpbackup` displays the contents of `stderr` to the user.

