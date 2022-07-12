# gpperfmon\_install 

Installs the `gpperfmon` database, which is used by Greenplum Command Center, and optionally enables the data collection agents.

## Synopsis 

```
gpperfmon_install --port <gpdb_port> 
      [--enable --password <gpmon_password> [--pgpass <path_to_file>]]
      [--verbose]

gpperfmon_install --help | -h | -?
```

## Description 

The `gpperfmon_install` utility automates the steps required to enable the data collection agents. You must be the Greenplum Database system user \(`gpadmin`\) to run this utility. The `--port` option is required. When using the `--enable` option, the `--password` option is also required. Use the `--port` option to supply the port of the Greenplum Database master instance. If using the `--enable` option, Greenplum Database must be restarted after the utility completes.

When run without the `--enable` option, the utility just creates the `gpperfmon` database \(the database used to store system metrics collected by the data collection agents\). When run with the `--enable` option, the utility also runs the following additional tasks necessary to enable the performance monitor data collection agents:

1.  Creates the `gpmon` superuser role in Greenplum Database. The data collection agents require this role to connect to the database and write their data. The `gpmon` superuser role uses MD5-encrypted password authentication by default. Use the `--password` option to set the `gpmon` superuser's password.
2.  Updates the `$MASTER_DATA_DIRECTORY/pg_hba.conf` file. The utility adds these lines to the host-based authentication file \(`pg_hba.conf`\):

    ```
    local      gpperfmon     gpmon                 md5
    host       all           gpmon  127.0.0.1/28   md5
    host       all           gpmon  ::1/128        md5
    ```

    The second and third lines, the `host` entries, give `gpmon` access to all Greenplum Database databases.

    **Note:** It might be necessary to edit the lines in the `pg_hba.conf` file after running the `gpperfmon_install` utility to limit the `gpmon` role's access to databases or to change the authentication method. After you edit the file, run `gpstop -u` to reload the file in Greenplum Database.

    -   To limit `gpmon` access to just the `gpperfmon` database, edit the `host` entries in the`pg_hba.conf` file. For the `gpmon` user change the second field from `all` to `gpperfmon`:

        ```
        local      gpperfmon     gpmon                  md5
        host       gpperfmon     gpmon    127.0.0.1/28  md5
        host       gpperfmon     gpmon    ::1/128       md5
        ```

    -   The `gpperfmon_install` utility assumes the default MD5 authentication method. Greenplum Database can optionally be configured to use the SHA-256 hash algorithm to compute the password hashes saved in the system catalog. This is incompatible with the MD5 authentication method, which expects an MD5 hash or clear text password in the system catalog. Because of this, if you have enabled the SHA-256 hash algorithm in the database, you must edit the `pg_hba.conf` file after running the `gpperfmon_install` utility. For the `host` entries, change the authentication method for the `gpmon` role from `md5` to `password`:

        ```
        local      gpperfmon     gpmon                 md5 
        host       all           gpmon  127.0.0.1/28   password
        host       all           gpmon  ::1/128        password
        ```

        The `password` authentication method submits the user's clear text password for authentication and should not be used on an untrusted network. See "Protecting Passwords in Greenplum Database" in the *Greenplum Database Administrator Guide* for more information about configuring password hashing.

3.  Updates the password file \(`.pgpass`\). In order to allow the data collection agents to connect as the `gpmon` role without a password prompt, you must have a password file that has an entry for the `gpmon` user. The utility adds the following entry to your password file \(if the file does not exist, the utility will create it\):

    ```
    *:5432:gpperfmon:gpmon:<gpmon_password>
    ```

    If your password file is not located in the default location \(`~/.pgpass`\), use the `--pgpass` option to specify the file location.

4.  Sets the server configuration parameters for Greenplum Command Center. The following parameters must be enabled for the data collection agents to begin collecting data. The utility sets the following parameters in the Greenplum Database `postgresql.conf` configuration files:

    -   `gp_enable_gpperfmon=on` \(in all `postgresql.conf` files\)
    -   `gpperfmon_port=8888` \(in all `postgresql.conf` files\)
    -   `gp_external_enable_exec=on` \(in the master `postgresql.conf` file\)
    Data collection agents can be configured by setting parameters in the `gpperfmon.conf` configuration file. See [Data Collection Agent Configuration](#section_p51_bxc_wz) for details.

    For information about Greenplum Command Center, see the [Greenplum Command Center Documentation](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Command-Center/index.html).


## Options 

--enable
:   In addition to creating the `gpperfmon` database, performs the additional steps required to enable the data collection agents. When `--enable` is specified the utility will also create and configure the `gpmon` superuser account and set the Command Center server configuration parameters in the `postgresql.conf` files.

--password gpmon\_password
:   Required if `--enable` is specified. Sets the password of the `gpmon` superuser. Disallowed if `--enable` is not specified.

--port gpdb\_port
:   Required. Specifies the connection port of the Greenplum Database master.

--pgpass path\_to\_file
:   Optional if `--enable` is specified. If the password file is not in the default location of `~/.pgpass`, specifies the location of the password file.

--verbose
:   Sets the logging level to verbose.

--help \| -h \| -?
:   Displays the online help.

## Data Collection Agent Configuration 

The `$MASTER_DATA_DIRECTORY/gpperfmon/conf/gpperfmon.conf` file stores configuration parameters for the data collection agents. For configuration changes to these options to take effect, you must save `gpperfmon.conf` and then restart Greenplum Database server \(`gpstop -r`\).

The `gpperfmon.conf` file contains the following configuration parameters.

|Parameter|Description|
|---------|-----------|
|log\_location|Specifies a directory location for `gpperfmon` log files. Default is `$MASTER_DATA_DIRECTORY/gpperfmon/logs`.|
|min\_query\_time|Specifies the minimum query run time in seconds for statistics collection. All queries that run longer than this value are logged in the `queries_history` table. For queries with shorter run times, no historical data is collected. Defaults to 20 seconds.<br/><br/>If you know that you want to collect data for all queries, you can set this parameter to a low value. Setting the minimum query run time to zero, however, collects data even for the numerous queries run by Greenplum Command Center, creating a large amount of data that may not be useful.
|max\_log\_size|This parameter is not included in `gpperfmon.conf`, but it may be added to this file.<br/><br/>To prevent the log files from growing to excessive size, you can add the `max_log_size` parameter to `gpperfmon.conf`. The value of this parameter is measured in bytes. For example:<br/><br/>`max_log_size = 10485760`<br/><br/>With this setting, the log files will grow to 10MB before the system rolls over to a new log file.|
|partition\_age|The number of months that `gpperfmon` statistics data will be retained. The default value is 0, which means that partitions are never dropped automatically.|
|quantum|Specifies the time in seconds between updates from data collection agents on all segments. Valid values are 10, 15, 20, 30, and 60. Defaults to 15 seconds.If you prefer a less granular view of performance, or want to collect and analyze minimal amounts of data for system metrics, choose a higher quantum. To collect data more frequently, choose a lower value.|
|harvest\_interval|The time, in seconds, between data harvests. A data harvest moves recent data from the `gpperfmon` external \(`_tail`\) tables to their corresponding history files. The default is 120. The minimum value is 30.|
|smdw\_aliases|This parameter allows you to specify additional host names for the standby master. For example, if the standby master has two NICs, you can enter:<br/><br/>`smdw_aliases=smdw-1,smdw-2`<br/><br/>This optional fault tolerance parameter is useful if the Greenplum Command Center loses connectivity with the standby master. Instead of continuously retrying to connect to host smdw, it will try to connect to the NIC-based aliases of `smdw-1` and/or `smdw-2`. This ensures that the Command Center Console can continuously poll and monitor the standby master.|

## Notes 

The `gpperfmon` database and Greenplum Command Center require the `gpmon` role. After the `gpperfmon` database and `gpmon` role have been created, you can change the password for the `gpmon` role and update the information that Greenplum Command Center uses to connect to the `gpperfmon` database:

1.  Log in to Greenplum Database as a superuser and change the `gpmon` password with the `ALTER ROLE` command.

    ```
    # ALTER ROLE gpmon WITH PASSWORD '<new_password>' ;
    ```

2.  Update the password in `.pgpass` file that is used by Greenplum Command Center. The default file location is the `gpadmin` home directory \(`~/.pgpass`\). The `.pgpass` file contains a line with the `gpmon` password.

    ```
    *:5432:gpperfmon:gpmon:<new_password>
    ```

3.  Restart the Greenplum Command Center with the Command Center `gpcmdr` utility.

    ```
    $ gpcmdr --restart
    ```


The `gpperfmon` monitoring system requires some initialization after startup. Monitoring information appears after a few minutes have passed, and not immediately after installation and startup of the `gpperfmon` system.

## Examples 

Create the `gpperfmon` database only:

```
$ su - gpadmin
$ gpperfmon_install --port 5432
```

Create the `gpperfmon` database, create the `gpmon` superuser, and enable the data collection agents:

```
$ su - gpadmin
$ gpperfmon_install --enable --password changeme --port 5432
$ gpstop -r
```

## See Also 

[gpstop](gpstop.html)

