---
title: plugin\_api\_version 
---

Plugin command to display the supported Backup Storage Plugin API version.

## <a id="section2"></a>Synopsis 

```
<plugin_executable> plugin_api_version
```

## <a id="section3"></a>Description 

`gpbackup` and `gprestore` invoke the `plugin_api_version` plugin command before a backup or restore operation to determine Backup Storage Plugin API version compatibility.

## <a id="section5"></a>Return Value 

The `plugin_api_version` command must return the Backup Storage Plugin API version number supported by the storage plugin, "0.4.0".

