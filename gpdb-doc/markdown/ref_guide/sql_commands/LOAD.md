# LOAD 

Loads or reloads a shared library file.

## Synopsis 

``` {#sql_command_synopsis}
LOAD '<filename>'
```

## Description 

This command loads a shared library file into the Greenplum Database server address space. If the file had been loaded previously, it is first unloaded. This command is primarily useful to unload and reload a shared library file that has been changed since the server first loaded it. To make use of the shared library, function\(s\) in it need to be declared using the `CREATE FUNCTION` command.

The file name is specified in the same way as for shared library names in `CREATE FUNCTION`; in particular, one may rely on a search path and automatic addition of the system's standard shared library file name extension.

Note that in Greenplum Database the shared library file \(`.so` file\) must reside in the same path location on every host in the Greenplum Database array \(masters, segments, and mirrors\).

Only database superusers can load shared library files.

## Parameters 

filename
:   The path and file name of a shared library file. This file must exist in the same location on all hosts in your Greenplum Database array.

## Examples 

Load a shared library file:

```
LOAD '/usr/local/greenplum-db/lib/myfuncs.so';
```

## Compatibility 

`LOAD` is a Greenplum Database extension.

## See Also 

[CREATE FUNCTION](CREATE_FUNCTION.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

