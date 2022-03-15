---
title: Using the DD Boost Storage Plugin with gpbackup, gprestore, and gpbackup\_manager 
---

**Note:** The DD Boost storage plugin is available only with the commercial release of Tanzu Greenplum Backup and Restore.

Dell EMC Data Domain Boost \(DD Boost\) is Dell EMC software that can be used with the [gpbackup](../../utility_guide/ref/gpbackup.html) and [gprestore](../../utility_guide/ref/gprestore.html) utilities to perform faster backups to the Dell EMC Data Domain storage appliance. You can also replicate a backup on a separate, remote Data Domain system for disaster recovery with gpbackup or [gpbackup\_manager](../../utility_guide/ref/gpbackup_manager.html). For information about replication, see [Replicating Backups](replication-ddb.html).

To use the DD Boost storage plugin application, you first create a configuration file to specify the location of the plugin, the DD Boost login, and the backup location. When you run `gpbackup` or `gprestore`, you specify the configuration file with the option `--plugin-config`. For information about the configuration file, see [DD Boost Storage Plugin Configuration File Format](#ddb-plugin-config).

If you perform a backup operation with the `gpbackup` option `--plugin-config`, you must also specify the `--plugin-config` option when you restore the backup with `gprestore`.

## <a id="ddb-plugin-config"></a>DD Boost Storage Plugin Configuration File Format 

The configuration file specifies the absolute path to the Greenplum Database DD Boost storage plugin executable, DD Boost connection credentials, and Data Domain location. The configuration file is required only on the master host. The DD Boost storage plugin application must be in the same location on every Greenplum Database host.

The DD Boost storage plugin configuration file uses the [YAML 1.1](http://yaml.org/spec/1.1/) document format and implements its own schema for specifying the DD Boost information.

The configuration file must be a valid YAML document. The `gpbackup` and `gprestore` utilities process the configuration file document in order and use indentation \(spaces\) to determine the document hierarchy and the relationships of the sections to one another. The use of white space is significant. White space should not be used simply for formatting purposes, and tabs should not be used at all.

This is the structure of a DD Boost storage plugin configuration file.

```
executablepath: <absolute-path-to-gpbackup_ddboost_plugin>
options: 
  hostname: "<data-domain-host>"
  username: "<ddboost-ID>"
  password_encryption: "on" | "off"       
  password: "<ddboost-pwd>"
  storage_unit: "<data-domain-id>"
  directory: "<data-domain-dir>"
  replication: "on" | "off"
  replication_streams: integer
  remote_hostname: "<remote-dd-host>"
  remote_username: "<remote-ddboost-ID>"
  remote_password_encryption "on" | "off"
  remote_password: "<remote-dd-pwd>"
  remote_storage_unit: "<remote-dd-ID>"
  remote_directory: "<remote-dd-dir>"
```

executablepath
:   Required. Absolute path to the plugin executable. For example, the Greenplum Database installation location is `$GPHOME/bin/gpbackup_ddboost_plugin`. The plugin must be in the same location on every Greenplum Database host.

options
:   Required. Begins the DD Boost storage plugin options section.

    hostname
    :   Required. The IP address or hostname of the host. There is a 30-character limit.

    username
    :   Required. The Data Domain Boost user name. There is a 30-character limit.

    password\_encryption
    :   Optional. Specifies whether the `password` option value is encrypted. Default value is `off`. Use the [gpbackup\_manager](../../utility_guide/ref/gpbackup_manager.html) `encrypt-password` command to encrypt the plain-text password for the DD Boost user. If the `replication` option is `on`, `gpbackup_manager` also encrypts the remote Data Domain user's password. Copy the encrypted password\(s\) from the `gpbackup_manager` output to the `password` options in the configuration file.

    password
    :   Required. The passcode for the DD Boost user to access the Data Domain storage unit. If the `password_encryption` option is `on`, this is an encrypted password.

    storage-unit
    :   Required. A valid storage unit name for the Data Domain system that is used for backup and restore operations.

    directory
    :   Required. The location for the backup files, configuration files, and global objects on the Data Domain system. The location on the system is `/<`data-domain-dir\> in the storage unit of the system.

    :   During a backup operation, the plugin creates the directory location if it does not exist in the storage unit and stores the backup in this directory `/<data-domain-dir>/YYYYMMDD/YYYYMMDDHHMMSS/`.

    replication
    :   Optional. Enables or disables backup replication with DD Boost managed file replication when `gpbackup` performs a backup operation. Value is either `on` or `off`. Default value is `off`, backup replication is disabled. When the value is `on`, the DD Boost plugin replicates the backup on the Data Domain system that you specify with the `remote_*` options.

    :   The `replication` option and `remote_*` options are ignored when performing a restore operation with `gprestore`. The `remote_*` options are ignored if `replication` is `off`.

    :   This option is ignored when you perform replication with the `gpbackup_manager replicate-backup` command. For information about replication,see [Replicating Backups](replication-ddb.html).

    replication\_streams
    :   Optional. Used with the `gpbackup_manager replicate-backup` command, ignored otherwise. Specifies the maximum number of Data Domain I/O streams that can be used when replicating a backup set on a remote Data Domain server from the Data Domain server that contains the backup. Default value is 1.

    :   This option is ignored when you perform replication with `gpbackup`. The default value is used.

    remote\_hostname
    :   Required when performing replication. The IP address or hostname of the Data Domain system that is used for remote backup storage. There is a 30-character limit.

    remote\_username
    :   Required when performing replication. The Data Domain Boost user name that accesses the remote Data Domain system. There is a 30-character limit.

    remote\_password\_encryption
    :   Optional when performing replication. Specifies whether the `remote_password` option value is encrypted. The default value is `off`. To set up password encryption use the [gpbackup\_manager](../../utility_guide/ref/gpbackup_manager.html) `encrypt-password` command to encrypt the plain-text passwords for the DD Boost user. If the `replication` parameter is `on`, `gpbackup_manager` also encrypts the remote Data Domain user's password. Copy the encrypted passwords from the `gpbackup_manager` output to the password options in the configuration file.

    remote\_password
    :   Required when performing replication. The passcode for the DD Boost user to access the Data Domain storage unit on the remote system. If the `remote_password_encryption` option is `on`, this is an encrypted password.

    remote\_storage\_unit
    :   Required when performing replication. A valid storage unit name for the remote Data Domain system that is used for backup replication.

    remote\_directory
    :   Required when performing replication. The location for the replicated backup files, configuration files, and global objects on the remote Data Domain system. The location on the system is `/<`remote-dd-dir\> in the storage unit of the remote system.

    :   During a backup operation, the plugin creates the directory location if it does not exist in the storage unit of the remote Data Domain system and stores the replicated backup in this directory `/<remote-dd-dir>/YYYYMMDD/YYYYMMDDHHMMSS/`.

## <a id="ddb_examples"></a>Examples 

This is an example DD Boost storage plugin configuration file that is used in the next `gpbackup` example command. The name of the file is `ddboost-test-config.yaml`.

```
executablepath: $GPHOME/bin/gpbackup_ddboost_plugin
options: 
  hostname: "192.0.2.230"
  username: "test-ddb-user"
  password: "asdf1234asdf"
  storage_unit: "gpdb-backup"
  directory: "test/backup"
```

This `gpbackup` example backs up the database demo using the DD Boost storage plugin. The absolute path to the DD Boost storage plugin configuration file is `/home/gpadmin/ddboost-test-config.yml`.

```
gpbackup --dbname demo --single-data-file --plugin-config /home/gpadmin/ddboost-test-config.yaml
```

The DD Boost storage plugin writes the backup files to this directory of the Data Domain storage unit `gpdb-backup`.

```
/test/backup/<YYYYMMDD>/<YYYYMMDDHHMMSS>/
```

This is an example DD Boost storage plugin configuration file that enables replication.

```
executablepath: $GPHOME/bin/gpbackup_ddboost_plugin
options:
  hostname: "192.0.2.230"
  username: "test-ddb-user"
  password: "asdf1234asdf"
  storage_unit: "gpdb-backup"
  directory: "test/backup"
  replication: "on"
  remote_hostname: "192.0.3.20"
  remote_username: "test-dd-remote"
  remote_password: "qwer2345erty"
  remote_storage_unit: "gpdb-remote"
  remote_directory: "test/replication"
```

To restore from the replicated backup in the previous example, you can run `gprestore` with the DD Boost storage plugin and specify a configuration file with this information.

```
executablepath: $GPHOME/bin/gpbackup_ddboost_plugin
options:
  hostname: "192.0.3.20"
  remote_username: "test-dd-remote"
  remote_password: "qwer2345erty"
  storage_unit: "gpdb-remote"
  directory: "test/replication"
```

## <a id="ddboost_notes"></a>Notes 

Dell EMC DD Boost is integrated with Tanzu Greenplum and requires a DD Boost license. Open source Greenplum Database cannot use the DD Boost software, but can back up to a Dell EMC Data Domain system mounted as an NFS share on the Greenplum master and segment hosts.

When you perform a backup with the DD Boost storage plugin, the plugin stores the backup files in this location in the Data Domain storage unit.

```
<directory>/backups/<datestamp>/<timestamp>
```

Where <directory\> is the location you specified in the DD Boost configuration file, and <datestamp\> and <timestamp\> are the backup date and time stamps.

