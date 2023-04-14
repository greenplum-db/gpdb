# SHOW 

Shows the value of a system configuration parameter.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
SHOW <configuration_parameter>

SHOW ALL
```

## <a id="section3"></a>Description 

`SHOW` displays the current settings of Greenplum Database system configuration parameters. You can set these parameters with the `SET` statement, or by editing the `postgresql.conf` configuration file of the Greenplum Database coordinator. Note that some parameters viewable by `SHOW` are read-only — their values can be viewed but not set. See the Greenplum Database Reference Guide for details.

## <a id="section4"></a>Parameters 

configuration\_parameter
:   The name of a system configuration parameter.

ALL
:   Shows the current value of all configuration parameters.

## <a id="section5"></a>Examples 

Show the current setting of the parameter `DateStyle`:

```
SHOW DateStyle;
 DateStyle
-----------
 ISO, MDY
(1 row)

```

Show the current setting of the parameter geqo:

```
SHOW geqo;
 geqo
------
 off
(1 row)
```

Show the current setting of all parameters:

```
SHOW ALL;
       name       | setting |                  description
------------------+---------+----------------------------------------------------
 application_name | psql    | Sets the application name to be reported in sta...
       .
       .
       .
 xmlbinary        | base64  | Sets how binary values are to be encoded in XML.
 xmloption        | content | Sets whether XML data in implicit parsing and s...
(331 rows)
```

## <a id="section6"></a>Compatibility 

`SHOW` is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[SET](SET.html), [RESET](RESET.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

