---
title: restore\_data 
---

Plugin command to stream data from the remote storage system to `stdout`.

## <a id="section2"></a>Synopsis 

```
<plugin_executable> restore_data <plugin_config_file> <data_filenamekey>
```

## <a id="section3"></a>Description 

`gprestore` invokes a plugin's `restore_data` or `restore_data_subset` command to restore a backup. `gprestore` invokes the `restore_data` plugin command on each segment host when restoring a compressed, multiple-data-file, or non-filtered streaming backup, or when the plugin does not support the `restore_data_subset` command.

The `restore_data` implementation should read a potentially large data file named or mapped to `data_filenamekey` from the remote storage system and write the contents to `stdout`. If the `backup_data` command modified the data in any way \(i.e. compressed\), `restore_data` should perform the reverse operation.

## <a id="section4"></a>Arguments 

plugin\_config\_file
:   The absolute path to the plugin configuration YAML file.

data\_filenamekey
:   The mapping key to a backup file on the remote storage system. data\_filenamekey is the same key provided to the `backup_data` command.

## <a id="section5"></a>Exit Code 

The `restore_data` command must exit with a value of 0 on success, non-zero if an error occurs. In the case of a non-zero exit code, `gprestore` displays the contents of `stderr` to the user.

## <a id="section6"></a>See Also 

[restore\_data\_subset](restore_data_subset.html)

