---
title: delete\_backup 
---

Plugin command to delete the directory for a given backup timestamp from a remote system.

## <a id="section2"></a>Synopsis 

```
<delete_backup> <plugin_config_file> <timestamp>
```

## <a id="section3"></a>Description 

`gpbackup_manager` invokes the `delete_backup` plugin command to delete the directory specified by the backup timestamp on the remote system.

## <a id="section4"></a>Arguments 

plugin\_config\_file
:   The absolute path to the plugin configuration YAML file.

timestamp
:   The timestamp for the backup to delete.

## <a id="section5"></a>Exit Code 

The `delete_backup` command must exit with a value of 0 on success, or a non-zero value if an error occurs. In the case of a non-zero exit code, `gpbackup_manager` displays the contents of `stderr` to the user.

## <a id="exls"></a>Example 

```
my_plugin delete_backup /home/my-plugin_config.yaml 20191208130802
```

