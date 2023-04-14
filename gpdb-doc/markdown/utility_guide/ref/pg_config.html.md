# pg_config 

Retrieves information about the installed version of Greenplum Database.

## <a id="section2"></a>Synopsis 

``` {#client_util_synopsis}
pg_config [<option> ...]

pg_config -? | --help

pg_config --version
```

## <a id="section3"></a>Description 

The `pg_config` utility prints configuration parameters of the currently installed version of Greenplum Database. It is intended, for example, to be used by software packages that want to interface to Greenplum Database to facilitate finding the required header files and libraries. Note that information printed out by `pg_config` is for the Greenplum Database coordinator only.

If more than one option is given, the information is printed in that order, one item per line. If no options are given, all available information is printed, with labels.

## <a id="section4"></a>Options 

--bindir
:   Print the location of user executables. Use this, for example, to find the `psql` program. This is normally also the location where the `pg_config` program resides.

--docdir
:   Print the location of documentation files.

--includedir
:   Print the location of C header files of the client interfaces.

--pkgincludedir
:   Print the location of other C header files.

--includedir-server
:   Print the location of C header files for server programming.

--libdir
:   Print the location of object code libraries.

--pkglibdir
:   Print the location of dynamically loadable modules, or where the server would search for them. \(Other architecture-dependent data files may also be installed in this directory.\)

--localedir
:   Print the location of locale support files.

--mandir
:   Print the location of manual pages.

--sharedir
:   Print the location of architecture-independent support files.

--sysconfdir
:   Print the location of system-wide configuration files.

--pgxs
:   Print the location of extension makefiles.

--configure
:   Print the options that were given to the configure script when Greenplum Database was configured for building.

--cc
:   Print the value of the CC variable that was used for building Greenplum Database. This shows the C compiler used.

--cppflags
:   Print the value of the `CPPFLAGS` variable that was used for building Greenplum Database. This shows C compiler switches needed at preprocessing time.

--cflags
:   Print the value of the `CFLAGS` variable that was used for building Greenplum Database. This shows C compiler switches.

--cflags\_sl
:   Print the value of the `CFLAGS_SL` variable that was used for building Greenplum Database. This shows extra C compiler switches used for building shared libraries.

--ldflags
:   Print the value of the `LDFLAGS` variable that was used for building Greenplum Database. This shows linker switches.

--ldflags\_ex
:   Print the value of the `LDFLAGS_EX` variable that was used for building Greenplum Database. This shows linker switches that were used for building executables only.

--ldflags\_sl
:   Print the value of the `LDFLAGS_SL` variable that was used for building Greenplum Database. This shows linker switches used for building shared libraries only.

--libs
:   Print the value of the `LIBS` variable that was used for building Greenplum Database. This normally contains `-l` switches for external libraries linked into Greenplum Database.

--version
:   Print the version of Greenplum Database.

## <a id="section5"></a>Examples 

To reproduce the build configuration of the current Greenplum Database installation, run the following command:

```
eval ./configure 'pg_config --configure'
```

The output of `pg_config --configure` contains shell quotation marks so arguments with spaces are represented correctly. Therefore, using `eval` is required for proper results.

