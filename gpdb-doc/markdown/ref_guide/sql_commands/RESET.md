# RESET 

Restores the value of a system configuration parameter to the default value.

## Synopsis 

``` {#sql_command_synopsis}
RESET <configuration_parameter>

RESET ALL
```

## Description 

`RESET` restores system configuration parameters to their default values. `RESET` is an alternative spelling for `SET configuration\_parameter TO DEFAULT`.

The default value is defined as the value that the parameter would have had, had no `SET` ever been issued for it in the current session. The actual source of this value might be a compiled-in default, the master `postgresql.conf` configuration file, command-line options, or per-database or per-user default settings. See [Server Configuration Parameters](../config_params/guc_config.html) for more information.

## Parameters 

configuration\_parameter
:   The name of a system configuration parameter. See [Server Configuration Parameters](../config_params/guc_config.html) for details.

ALL
:   Resets all settable configuration parameters to their default values.

## Examples 

Set the `statement_mem` configuration parameter to its default value:

```
RESET statement_mem; 
```

## Compatibility 

`RESET` is a Greenplum Database extension.

## See Also 

[SET](SET.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

