---
title: backup\_data 
---

Plugin command to move streaming data from `stdin` to the remote storage system.

## <a id="section2"></a>Synopsis 

```
<plugin_executable> backup_data <plugin_config_file> <data_filenamekey>
```

## <a id="section3"></a>Description 

`gpbackup` invokes the `backup_data` plugin command on each segment host during a streaming backup.

The `backup_data` implementation should read a potentially large stream of data from `stdin` and write the data to a single file on the remote storage system. The data is sent to the command as a single continuous stream per Greenplum Database segment. If `backup_data` modifies the data in any manner \(i.e. compresses\), `restore_data` must perform the reverse operation.

Name or maintain a mapping from the destination file to `data_filenamekey`. This will be the file key used for the restore operation.

## <a id="section4"></a>Arguments 

plugin\_config\_file
:   The absolute path to the plugin configuration YAML file.

data\_filenamekey
:   The mapping key for a specially-named backup file for streamed data.

## <a id="section5"></a>Exit Code 

The `backup_data` command must exit with a value of 0 on success, non-zero if an error occurs. In the case of a non-zero exit code, `gpbackup` displays the contents of `stderr` to the user.

