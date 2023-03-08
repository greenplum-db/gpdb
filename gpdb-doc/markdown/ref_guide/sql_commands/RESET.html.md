# RESET 

Restores the value of a system configuration parameter to the default value.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
RESET <configuration_parameter>

RESET ALL
```

## <a id="section3"></a>Description 

`RESET` restores system configuration parameters to their default values. `RESET` is an alternative spelling for `SET configuration\_parameter TO DEFAULT`.

The default value is defined as the value that the parameter would have had, had no `SET` ever been issued for it in the current session. The actual source of this value might be a compiled-in default, the coordinator `postgresql.conf` configuration file, command-line options, or per-database or per-user default settings. See [Server Configuration Parameters](../config_params/guc_config.html) for more information.

## <a id="section4"></a>Parameters 

configuration\_parameter
:   The name of a system configuration parameter. See [Server Configuration Parameters](../config_params/guc_config.html) for details.

ALL
:   Resets all settable configuration parameters to their default values.

## <a id="section5"></a>Examples 

Set the `statement_mem` configuration parameter to its default value:

```
RESET statement_mem; 
```

## <a id="section6"></a>Compatibility 

`RESET` is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[SET](SET.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

