# SHOW 

Shows the value of a system configuration parameter.

## Synopsis 

``` {#sql_command_synopsis}
SHOW <configuration_parameter>

SHOW ALL
```

## Description 

`SHOW` displays the current settings of Greenplum Database system configuration parameters. You can set these parameters with the `SET` statement, or by editing the `postgresql.conf` configuration file of the Greenplum Database master. Note that some parameters viewable by `SHOW` are read-only — their values can be viewed but not set. See the Greenplum Database Reference Guide for details.

## Parameters 

configuration\_parameter
:   The name of a system configuration parameter.

ALL
:   Shows the current value of all configuration parameters.

## Examples 

Show the current setting of the parameter `search_path`:

```
SHOW search_path;
```

Show the current setting of all parameters:

```
SHOW ALL;
```

## Compatibility 

`SHOW` is a Greenplum Database extension.

## See Also 

[SET](SET.html), [RESET](RESET.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

