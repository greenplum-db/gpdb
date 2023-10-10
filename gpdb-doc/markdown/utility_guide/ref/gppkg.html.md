# gppkg 

Greenplum Package Manager installs, upgrades, migrates, and removes Greenplum Database extensions in `.gppkg` format, such as PL/Java, PL/R, PostGIS, and MADlib, along with their dependencies, across an entire cluster.

## <a id="synopsis"></a>Synopsis 

```
gppkg <command> [<command_options> ...] 

gppkg <commmand> --h | --help

gppkg --version

gppkg -v | --verbose
```

## <a id="description"></a>Description 

The Greenplum Package Manager -- `gppkg` -- utility installs Greenplum Database extensions, along with any dependencies, on all hosts across a cluster. It will also automatically install extensions on new hosts in the case of system expansion and segment recovery.

The `gppkg` utility does not require that Greenplum Database is running in order to install packages.

> **Note** After a major upgrade to Greenplum Database, you must download and install all `gppkg` extensions again.

Examples of database extensions and packages software that are delivered using the Greenplum Package Manager:

-   PL/Java
-   PL/R
-   PostGIS
-   MADlib

## <a id="commands"></a>Commands

help 
:   Display the help for the command.

install <package_name> [<command_options>]
:   Install or upgrade the specified package in the cluster. This includes any pre/post installation steps and installation of any dependencies.

migrate --source <source_path> --destination <destination_path> [--pkglibs <pkglibs_path>] [<command_options>]
:   Migrate all packages from one minor version of Greenplum Database to another. The option `--source <source_path>` specifies the path of the source `$GPHOME`, and the option `--destination <destination_path>` specifies the path of the destination `$GPHOME`. Additionally, the option `--pkglibs <pkglibs_path>` allows you to point to a location where you may place newer version packages for the destination Greenplum version; `gppkg` will upgrade these packages automatically. 

query [<package_name_string>] [<query_option>] [<command_options>]
:   Display information about the extensions installed in the cluster. `<package_name_string>` is a string that specifies the package name. If it is an empty string, it will match all packages. If it is a simple word, it will match all packages with the word included in the name. Use `–-exact` to specify the exact package name.

    |query_option|Returns|
    |-------------|-------|
    |`--exact`|The provided `<package_name_string>` must match exactly a package name|
    |`--detail`|Provide detailed information about the package|
    |`--verify`|Verify the package installation|
    |`--local`|Do not query at cluster level|

remove <package_name> [<command_options>]
:    Uninstall the specified package from the cluster. 

sync [<command_options>]
:    Reconcile the package state of the cluster to match the state of the master host. Running this option after a failed or partial install/uninstall ensures that the package installation state is consistent across the cluster.

## <a id="options"></a>Global Options 

--cluster_info <cluster_info>
:   Use this option when Greenplum Database is not running. The input file `<cluster_info>` contains information about the database cluster. You may generate the file by running the following command:

    ```
    psql postgres -Xc 'select dbid, content, role, preferred_role, mode, status, hostname, address, port, datadir from gp_segment_configuration order by content, preferred_role desc;' | head -n-2 | tail -n+3 | tr -d " " > cluster_info
    ```

-a | --accept 
:   Do not prompt the user for confirmation.

-d | --dryrun     
:   Run a simulation for the command, without modifying anything.

-f | --force
:   Skip all requirement checks and overwrite existing files.

-h | --help
:   Display the online help.

--tmpdir
:   Specify the directory to which `gppkg` should write temporary files. If not specified, `gppkg` writes temporary files to the directory specified in the `TMPDIR` environment variable.

-V | --version
:   Display the version of this utility.

-v | --verbose
:   Set the logging level to verbose.

## <a id="examples"></a>Examples

Install the Greenplum Database PL/Java extension:

```
gppkg install ./pljava-2.0.7-gp7-rhel8_x86_64.gppkg
```

Migrate all installed packages from Greenplum Database version 7.0.0 to Greenplum Database version 7.1.0 while the cluster is not running.

```
gppkg migrate --cluster-info ./cluster_info --source /usr/local/greenplum-db-7.0.0 --destination /usr/local/greenplum-db-7.1.0
```

where the file `./cluster_info` contains the following information:

```
1|-1|p|p|n|u|cdw|cdw|5432|/data/coordinator/gpseg-1
2|0|p|p|s|u|seg-02|sdw1|6000|/data/primary/gpseg0
6|0|m|m|s|u|seg-03|sdw2|7000|/data/mirror/gpseg0
3|1|p|p|s|u|seg-02|sdw1|6001|/data/primary/gpseg1
7|1|m|m|s|u|seg-03|sdw2|7001|/data/mirror/gpseg1
```

Query all packages that are installed in a cluster:

```
gppkg query 

Detecting network topology:    [=========================================] [OK] 
Detect result 
 3 unique hosts found 
DataSciencePython3.9 - 1.1.0 
```

