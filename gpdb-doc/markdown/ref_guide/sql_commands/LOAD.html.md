# LOAD 

Loads or reloads a shared library file.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
LOAD '<filename>'
```

## <a id="section3"></a>Description 

This command loads a shared library file into the Greenplum Database server address space. If the file had been loaded previously, it is first unloaded. This command is primarily useful to unload and reload a shared library file that has been changed since the server first loaded it. To make use of the shared library, function\(s\) in it need to be declared using the `CREATE FUNCTION` command.

The library file name is typically given as just a bare file name, which is sought in the server's library search path (set by `dynamic_library_path`). Alternatively it can be given as a full path name. In either case the platform's standard shared library file name extension may be omitted.

Note that in Greenplum Database the shared library file \(`.so` file\) must reside in the same path location on every host in the Greenplum Database array \(coordinators, segments, and mirrors\).

Non-superusers can only apply `LOAD` to library files located in `$libdir/plugins/` — the specified `filename` must begin with exactly that string. You must ensure that only “safe” libraries are installed there.

## <a id="section4"></a>Parameters 

filename
:   The path and file name of a shared library file. This file must exist in the same location on all hosts in your Greenplum Database array.

## <a id="section5"></a>Examples 

Load a shared library file:

```
LOAD '/usr/local/greenplum-db/lib/myfuncs.so';
```

## <a id="section6"></a>Compatibility 

`LOAD` is a Greenplum Database extension.

## <a id="section7"></a>See Also 

[CREATE FUNCTION](CREATE_FUNCTION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

