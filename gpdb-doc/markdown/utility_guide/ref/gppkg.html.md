# gppkg 

Installs Greenplum Database extensions in `.gppkg` format, such as PL/Java, PL/R, PostGIS, and MADlib, along with their dependencies, across an entire cluster.

## <a id="section2"></a>Synopsis 

```
gppkg [-i <package> | -u <package> | -r  <name>-<version> | -c] 
        [-d <coordinator_data_directory>] [-a] [-v]

gppkg --migrate <GPHOME_old> <GPHOME_new> [-a] [-v]

gppkg [-q | --query] <query_option>

gppkg -? | --help | -h 

gppkg --version
```

## <a id="section3"></a>Description 

The Greenplum Package Manager \(`gppkg`\) utility installs Greenplum Database extensions, along with any dependencies, on all hosts across a cluster. It will also automatically install extensions on new hosts in the case of system expansion and segment recovery.

> **Note** After a major upgrade to Greenplum Database, you must download and install all `gppkg` extensions again.

Examples of database extensions and packages software that are delivered using the Greenplum Package Manager are:

-   PL/Java
-   PL/R
-   PostGIS
-   MADlib

## <a id="section4"></a>Options 

-a \(do not prompt\)
:   Do not prompt the user for confirmation.

-c \| --clean
:   Reconciles the package state of the cluster to match the state of the coordinator host. Running this option after a failed or partial install/uninstall ensures that the package installation state is consistent across the cluster.

-d coordinator\_data\_directory
:   The coordinator data directory. If not specified, the value set for `$COORDINATOR_DATA_DIRECTORY` will be used.

-i package \| --install=package
:   Installs the given package. This includes any pre/post installation steps and installation of any dependencies.

--migrate GPHOME\_old GPHOME\_new
:   Migrates packages from a separate `$GPHOME`. Carries over packages from one version of Greenplum Database to another.

:   For example: `gppkg --migrate /usr/local/greenplum-db-<old-version> /usr/local/greenplum-db-<new-version>`

:   > **Note** In general, it is best to avoid using the `--migrate` option, and packages should be reinstalled, not migrated. See [Upgrading from 6.x to a Newer 6.x Release](../../install_guide/upgrading.html#topic17).

:   When migrating packages, these requirements must be met.

    -   At least the coordinator instance of the destination Greenplum Database must be started \(the instance installed in GPHOME\_new\). Before running the `gppkg` command start the Greenplum Database coordinator with the command `gpstart -m`.
    -   Run the `gppkg` utility from the GPHOME\_new installation. The migration destination installation directory.

-q \| --query query\_option
:   Provides information specified by `query_option` about the installed packages. Only one `query_option` can be specified at a time. The following table lists the possible values for query\_option. `<package_file>` is the name of a package.

    |query\_option|Returns|
    |-------------|-------|
    |`<package_file>`|Whether the specified package is installed.|
    |`--info <package_file>`|The name, version, and other information about the specified package.|
    |`--list <package_file>`|The file contents of the specified package.|
    |`--all`|List of all installed packages.|

-r name-version \| --remove=name-version
:   Removes the specified package.

-u package \| --update=package
:   Updates the given package.

    > **Caution** The process of updating a package includes removing all previous versions of the system objects related to the package. For example, previous versions of shared libraries are removed. After the update process, a database function will fail when it is called if the function references a package file that has been removed.

:   --version \(show utility version\)
:   Displays the version of this utility.

-v \| --verbose
:   Sets the logging level to verbose.

-? \| -h \| --help
:   Displays the online help.

