---
title: restore\_file 
---

Plugin command to move a backup file from the remote storage system.

## <a id="section2"></a>Synopsis 

```
<plugin_executable> restore_file <plugin_config_file> <file_to_restore>
```

## <a id="section3"></a>Description 

`gprestore` invokes the `restore_file` plugin command on the master and each segment host for the file that `gprestore` will read from a backup directory on local disk.

The `restore_file` command should process and move the file from the remote storage system to `file\_to\_restore` on the local host.

## <a id="section4"></a>Arguments 

plugin\_config\_file
:   The absolute path to the plugin configuration YAML file.

file\_to\_restore
:   The absolute path to which to move a backup file from the remote storage system.

## <a id="section5"></a>Exit Code 

The `restore_file` command must exit with a value of 0 on success, non-zero if an error occurs. In the case of a non-zero exit code, `gprestore` displays the contents of `stderr` to the user.

