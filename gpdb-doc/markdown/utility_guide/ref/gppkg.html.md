# gppkg 

Installs Greenplum Database extensions in `.gppkg` format, such as PL/Java, PL/R, PostGIS, and MADlib, along with their dependencies, across an entire cluster.

## <a id="synopsis"></a>Synopsis 

```
gppkg <command> [<command_options> ...] 

gppkg <commmand> --h | --help

gppkg --version

gppkg -v | --verbose
```

## <a id="description"></a>Description 

The Greenplum Package Manager -- `gppkg` -- utility installs Greenplum Database extensions, along with any dependencies, on all hosts across a cluster. It will also automatically install extensions on new hosts in the case of system expansion and segment recovery.

The `gppkg` utility no does not require that a Greenplum Database be running in order to install packages.

> **Note** After a major upgrade to Greenplum Database, you must download and install all `gppkg` extensions again.

Examples of database extensions and packages software that are delivered using the Greenplum Package Manager:

-   PL/Java
-   PL/R
-   PostGIS
-   MADlib

## <a id="commands"></a>Commands

help 
:   Displays the online help for the command.

install [--tmpdir] <package-name>
:   Installs the given package in the cluster. This includes any pre/post installation steps and installation of any dependencies.

remove <package-name>
:    Uninstalls the given package from the cluster. 

## <a id="options"></a>Options 

-r-tempdir <dir-name>
:  Use with `gppkg install`. Specifies 

--version
:   Displays the version of this utility.

-v | --verbose
:   Sets the logging level to verbose.

