# gpconfig 

Sets server configuration parameters on all segments within a Greenplum Database system.

## Synopsis 

```
gpconfig -c <param_name> -v <value> [-m <master_value> | --masteronly]
       | -r <param_name> [--masteronly]
       | -l
       [--skipvalidation] [--verbose] [--debug]

gpconfig -s <param_name> [--file | --file-compare] [--verbose] [--debug]

gpconfig --help
```

## Description 

The `gpconfig` utility allows you to set, unset, or view configuration parameters from the `postgresql.conf` files of all instances \(master, segments, and mirrors\) in your Greenplum Database system. When setting a parameter, you can also specify a different value for the master if necessary. For example, parameters such as `max_connections` require a different setting on the master than what is used for the segments. If you want to set or unset a global or master only parameter, use the `--masteronly` option.

`gpconfig` can only be used to manage certain parameters. For example, you cannot use it to set parameters such as `port`, which is required to be distinct for every segment instance. Use the `-l` \(list\) option to see a complete list of configuration parameters supported by `gpconfig`.

When `gpconfig` sets a configuration parameter in a segment `postgresql.conf` file, the new parameter setting always displays at the bottom of the file. When you use `gpconfig` to remove a configuration parameter setting, `gpconfig` comments out the parameter in all segment `postgresql.conf` files, thereby restoring the system default setting. For example, if you use `gpconfig`to remove \(comment out\) a parameter and later add it back \(set a new value\), there will be two instances of the parameter; one that is commented out, and one that is enabled and inserted at the bottom of the `postgresql.conf` file.

After setting a parameter, you must restart your Greenplum Database system or reload the `postgresql.conf` files in order for the change to take effect. Whether you require a restart or a reload depends on the parameter.

For more information about the server configuration parameters, see the *Greenplum Database Reference Guide*.

To show the currently set values for a parameter across the system, use the `-s` option.

`gpconfig` uses the following environment variables to connect to the Greenplum Database master instance and obtain system configuration information:

-   `PGHOST`
-   `PGPORT`
-   `PGUSER`
-   `PGPASSWORD`
-   `PGDATABASE`

## Options 

-c \| --change param\_name
:   Changes a configuration parameter setting by adding the new setting to the bottom of the `postgresql.conf` files.

-v \| --value value
:   The value to use for the configuration parameter you specified with the `-c` option. By default, this value is applied to all segments, their mirrors, the master, and the standby master.

:   Parameter values that are not simple identifiers or numbers must be enclosed in single quotes \(`'`\). For example, values that contain spaces or special characters must be enclosed in single quotes. To embed a single quote in a parameter value, enter either two single quotes or backslash and quote \(`\'`\).

:   The utility encloses the value in single quotes when adding the setting to the `postgresql.conf` files.

:   To set the value to an empty string, enter empty single quotes \(`''`\).

-m \| --mastervalue master\_value
:   The master value to use for the configuration parameter you specified with the `-c` option. If specified, this value only applies to the master and standby master. This option can only be used with `-v`.

--masteronly
:   When specified, `gpconfig` will only edit the master `postgresql.conf` file.

-r \| --remove param\_name
:   Removes a configuration parameter setting by commenting out the entry in the `postgresql.conf` files.

-l \| --list
:   Lists all configuration parameters supported by the `gpconfig` utility.

-s \| --show param\_name
:   Shows the value for a configuration parameter used on all instances \(master and segments\) in the Greenplum Database system. If there is a difference in a parameter value among the instances, the utility displays an error message. Running `gpconfig` with the `-s` option reads parameter values directly from the database, and not the `postgresql.conf` file. If you are using `gpconfig` to set configuration parameters across all segments, then running `gpconfig -s` to verify the changes, you might still see the previous \(old\) values. You must reload the configuration files \(`gpstop -u`\) or restart the system \(`gpstop -r`\) for changes to take effect.

--file
:   For a configuration parameter, shows the value from the `postgresql.conf` file on all instances \(master and segments\) in the Greenplum Database system. If there is a difference in a parameter value among the instances, the utility displays a message. Must be specified with the `-s` option.

:   For example, the configuration parameter `statement_mem` is set to 64MB for a user with the `ALTER ROLE` command, and the value in the `postgresql.conf` file is 128MB. Running the command `gpconfig -s statement_mem --file` displays 128MB. The command `gpconfig -s statement_mem` run by the user displays 64MB.

:   Not valid with the `--file-compare` option.

--file-compare
:   For a configuration parameter, compares the current Greenplum Database value with the value in the `postgresql.conf` files on hosts \(master and segments\). The values in the `postgresql.conf files` represent the value when Greenplum Database is restarted.

:   If the values are not the same, the utility displays the values from all hosts. If all hosts have the same value, the utility displays a summary report.

:   Not valid with the `--file` option.

--skipvalidation
:   Overrides the system validation checks of `gpconfig` and allows you to operate on any server configuration parameter, including hidden parameters and restricted parameters that cannot be changed by `gpconfig`. When used with the `-l` option \(list\), it shows the list of restricted parameters.

    **Warning:** Use extreme caution when setting configuration parameters with this option.

--verbose
:   Displays additional log information during `gpconfig` command execution.

--debug
:   Sets logging output to debug level.

-? \| -h \| --help
:   Displays the online help.

## Examples 

Set the `gp_snmp_community` parameter to `testenv` in the master host file only:

```
gpconfig -c gp_snmp_community -v testenv --masteronly
```

Set the `max_connections` setting to 100 on all segments and 10 on the master:

```
gpconfig -c max_connections -v 100 -m 10
```

Set the server configuration parameters `gp_email_to` and `gp_email_from`. These parameters require single quotes around the values.

```
$ gpconfig -c gp_email_from -v 'gpdb-server@example.com'
$ gpconfig -c gp_email_to -v 'gpdb-admin@example.com'
```

In the `postgresql.conf` file, the parameters are set correctly, with single quotes around the values:

```
gp_email_from='gpdb-server@example.com'
gp_email_to='gpdb-admin@example.com'
```

These examples shows the syntax required due to bash shell string processing.

```
gpconfig -c search_path -v '"\$user",public'
gpconfig -c dynamic_library_path -v '\$libdir'
```

The configuration parameters are added to the `postgresql.conf` file.

```
search_path='"$user",public'
dynamic_library_path='$libdir'
```

Comment out all instances of the `default_statistics_target` configuration parameter, and restore the system default:

```
gpconfig -r default_statistics_target
```

List all configuration parameters supported by `gpconfig`:

```
gpconfig -l
```

Show the values of a particular configuration parameter across the system:

```
gpconfig -s max_connections
```

## See Also 

[gpstop](gpstop.html)

