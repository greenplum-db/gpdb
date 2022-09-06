# plcontainer 

The `plcontainer` utility installs Docker images and manages the PL/Container configuration. The utility consists of two sets of commands.

-   `image-*` commands manage Docker images on the Greenplum Database system hosts.
-   `runtime-*` commands manage the PL/Container configuration file on the Greenplum Database instances. You can add Docker image information to the PL/Container configuration file including the image name, location, and shared folder information. You can also edit the configuration file.

To configure PL/Container to use a Docker image, you install the Docker image on all the Greenplum Database hosts and then add configuration information to the PL/Container configuration.

PL/Container configuration values, such as image names, runtime IDs, and parameter values and names are case sensitive.

## <a id="synt"></a>plcontainer Syntax 

```
plcontainer [<command>] [-h | --help]  [--verbose]
```

Where <command\> is one of the following.

```
  image-add {{-f | --file} <image_file> [-ulc | --use_local_copy]} | {{-u | --URL} <image_URL>}
  image-delete {-i | --image} <image_name>
  image-list

  runtime-add {-r | --runtime} <runtime_id>
     {-i | --image} <image_name> {-l | --language} {python | python3 | r}
     [{-v | --volume} <shared_volume> [{-v| --volume} <shared_volume>...]]
     [{-s | --setting} <param=value> [{-s | --setting} <param=value> ...]]
  runtime-replace {-r | --runtime} <runtime_id>
     {-i | --image} <image_name> -l {r | python}
     [{-v | --volume} <shared_volume> [{-v | --volume} <shared_volume>...]]
     [{-s | --setting} <param=value> [{-s | --setting} <param=value> ...]]
  runtime-show {-r | --runtime} <runtime_id>
  runtime-delete {-r | --runtime} <runtime_id>
  runtime-edit [{-e | --editor} <editor>]
  runtime-backup {-f | --file} <config_file>
  runtime-restore {-f | --file} <config_file>
  runtime-verify
```

## <a id="cmds"></a>plcontainer Commands and Options 

image-add location
:   Install a Docker image on the Greenplum Database hosts. Specify either the location of the Docker image file on the host or the URL to the Docker image. These are the supported location options:

    -   \{**-f** \| **--file**\} image\_file Specify the file system location of the Docker image tar archive file on the local host. This example specifies an image file in the `gpadmin` user's home directory: `/home/gpadmin/test_image.tar.gz`
    -   \{**-u** \| **--URL**\} image\_URL Specify the URL of the Docker repository and image. This example URL points to a local Docker repository `192.168.0.1:5000/images/mytest_plc_r:devel`

:   By default, the `image-add` command copies the image to each Greenplum Database segment and standby master host, and installs the image. When you specify an image\_file and provide the \[**-ulc** \| **--use\_local\_copy**\] option, `plcontainer` installs the image only on the host on which you run the command.

:   After installing the Docker image, use the [runtime-add](#runtime_add) command to configure PL/Container to use the Docker image.

image-delete \{**-i** \| **--image**\} image\_name
:   Remove an installed Docker image from all Greenplum Database hosts. Specify the full Docker image name including the tag for example `pivotaldata/plcontainer_python_shared:1.0.0`

image-list
:   List the Docker images installed on the host. The command list only the images on the local host, not remote hosts. The command lists all installed Docker images, including images installed with Docker commands.

runtime-add options
:   Add configuration information to the PL/Container configuration file on all Greenplum Database hosts. If the specified runtime\_id exists, the utility returns an error and the configuration information is not added.

:   These are the supported options:

:   \{**-i** \| **--image**\} docker-image
:   Required. Specify the full Docker image name, including the tag, that is installed on the Greenplum Database hosts. For example `pivotaldata/plcontainer_python:1.0.0`.

:   The utility returns a warning if the specified Docker image is not installed.

:   The `plcontainer image-list` command displays installed image information including the name and tag \(the Repository and Tag columns\).

\{**-l** \| **--language**\} python \| python3 \| r
:   Required. Specify the PL/Container language type, supported values are `python` \(PL/Python using Python 2\), `python3` \(PL/Python using Python 3\) and `r` \(PL/R\). When adding configuration information for a new runtime, the utility adds a startup command to the configuration based on the language you specify.

:   Startup command for the Python 2 language.

    ```
    /clientdir/pyclient.sh
    ```

:   Startup command for the Python 3 language.

    ```
    /clientdir/pyclient3.sh
    ```

:   Startup command for the R language.

    ```
    /clientdir/rclient.sh
    ```

\{**-r** \| **--runtime**\} runtime\_id
:   Required. Add the runtime ID. When adding a `runtime` element in the PL/Container configuration file, this is the value of the `id` element in the PL/Container configuration file. Maximum length is 63 Bytes.

:   You specify the name in the Greenplum Database UDF on the `# container` line.

\{**-s** \| **--setting**\} param=value
:   Optional. Specify a setting to add to the runtime configuration information. You can specify this option multiple times. The setting applies to the runtime configuration specified by the runtime\_id. The parameter is the XML attribute of the [settings](#plc_settings) element in the PL/Container configuration file. These are valid parameters.

    -   `cpu_share` - Set the CPU limit for each container in the runtime configuration. The default value is 1024. The value is a relative weighting of CPU usage compared to other containers.
    -   `memory_mb` - Set the memory limit for each container in the runtime configuration. The default value is 1024. The value is an integer that specifies the amount of memory in MB.
    -   `resource_group_id` - Assign the specified resource group to the runtime configuration. The resource group limits the total CPU and memory resource usage for all containers that share this runtime configuration. You must specify the `groupid` of the resource group. For information about managing PL/Container resources, see [About PL/Container Resource Management](#topic_resmgmt).
    -   `roles` - Specify the Greenplum Database roles that are allowed to run a container for the runtime configuration. You can specify a single role name or comma separated lists of role names. The default is no restriction.
    -   `use_container_logging` - Enable or deactivate Docker logging for the container. The value is either `yes` \(activate logging\) or `no` \(deactivate logging, the default\).
    <br/><br/>The Greenplum Database server configuration parameter [log\_min\_messages](../../ref_guide/config_params/guc-list.html) controls the log level. The default log level is `warning`. For information about PL/Container log information, see [Notes](#plc_notes).


\{**-v** \| **--volume**\} shared-volume
:   Optional. Specify a Docker volume to bind mount. You can specify this option multiple times to define multiple volumes.

:   The format for a shared volume: `host-dir:container-dir:[rw|ro]`. The information is stored as attributes in the `shared_directory` element of the `runtime` element in the PL/Container configuration file.

    -   host-dir - absolute path to a directory on the host system. The Greenplum Database administrator user \(gpadmin\) must have appropriate access to the directory.
    -   container-dir - absolute path to a directory in the Docker container.
    -   `[rw|ro]` - read-write or read-only access to the host directory from the container.

:   When adding configuration information for a new runtime, the utility adds this read-only shared volume information.

:   `<greenplum-home>/bin/plcontainer_clients:/clientdir:ro`

:   If needed, you can specify other shared directories. The utility returns an error if the specified container-dir is the same as the one that is added by the utility, or if you specify multiple shared volumes with the same container-dir.

    **Warning:** Allowing read-write access to a host directory requires special considerations.

    -   When specifying read-write access to host directory, ensure that the specified host directory has the correct permissions.
    -   When running PL/Container user-defined functions, multiple concurrent Docker containers that are running on a host could change data in the host directory. Ensure that the functions support multiple concurrent access to the data in the host directory.

runtime-backup \{**-f** \| **--file**\} config\_file
:   Copies the PL/Container configuration file to the specified file on the local host.

runtime-delete \{**-r** \| **--runtime**\} runtime\_id
:   Removes runtime configuration information in the PL/Container configuration file on all Greenplum Database instances. The utility returns a message if the specified runtime\_id does not exist in the file.

runtime-edit \[\{**-e** \| **--editor**\} editor\]
:   Edit the XML file `plcontainer_configuration.xml` with the specified editor. The default editor is `vi`.

    Saving the file updates the configuration file on all Greenplum Database hosts. If errors exist in the updated file, the utility returns an error and does not update the file.

runtime-replace options
:   Replaces runtime configuration information in the PL/Container configuration file on all Greenplum Database instances. If the runtime\_id does not exist, the information is added to the configuration file. The utility adds a startup command and shared directory to the configuration.

    See [runtime-add](#runtime_add) for command options and information added to the configuration.

runtime-restore \{**-f** \| **--file**\} config\_file
:   Replaces information in the PL/Container configuration file `plcontainer_configuration.xml` on all Greenplum Database instances with the information from the specified file on the local host.

runtime-show \[\{**-r** \| **--runtime**\} runtime\_id\]
:   Displays formatted PL/Container runtime configuration information. If a runtime\_id is not specified, the configuration for all runtime IDs are displayed.

runtime-verify
:   Checks the PL/Container configuration information on the Greenplum Database instances with the configuration information on the master. If the utility finds inconsistencies, you are prompted to replace the remote copy with the local copy. The utility also performs XML validation.

**-h** \| **--help**
:   Display help text. If specified without a command, displays help for all `plcontainer` commands. If specified with a command, displays help for the command.

**--verbose**
:   Enable verbose logging for the command.

## <a id="exs"></a>Examples 

These are examples of common commands to manage PL/Container:

-   Install a Docker image on all Greenplum Database hosts. This example loads a Docker image from a file. The utility displays progress information on the command line as the utility installs the Docker image on all the hosts.

    ```
    plcontainer image-add -f plc_newr.tar.gz
    ```

    After installing the Docker image, you add or update a runtime entry in the PL/Container configuration file to give PL/Container access to the Docker image to start Docker containers.

-   Install the Docker image only on the local Greenplum Database host:

    ```
    plcontainer image-add -f /home/gpadmin/plc_python_image.tar.gz --use_local_copy
    ```

-   Add a container entry to the PL/Container configuration file. This example adds configuration information for a PL/R runtime, and specifies a shared volume and settings for memory and logging.

    ```
    plcontainer runtime-add -r runtime2 -i test_image2:0.1 -l r \
      -v /host_dir2/shared2:/container_dir2/shared2:ro \
      -s memory_mb=512 -s use_container_logging=yes
    ```

    The utility displays progress information on the command line as it adds the runtime configuration to the configuration file and distributes the updated configuration to all instances.

-   Show specific runtime with given runtime id in configuration file

    ```
    plcontainer runtime-show -r plc_python_shared
    ```

    The utility displays the configuration information similar to this output.

    ```
    PL/Container Runtime Configuration:
    ---------------------------------------------------------
     Runtime ID: plc_python_shared
     Linked Docker Image: test1:latest
     Runtime Setting(s):
     Shared Directory:
     ---- Shared Directory From HOST '/usr/local/greenplum-db/bin/plcontainer_clients' to Container '/clientdir', access mode is 'ro'
     ---- Shared Directory From HOST '/home/gpadmin/share/' to Container '/opt/share', access mode is 'rw'
    ---------------------------------------------------------
    ```

-   Edit the configuration in an interactive editor of your choice. This example edits the configuration file with the vim editor.

    ```
    plcontainer runtime-edit -e vim
    ```

    When you save the file, the utility displays progress information on the command line as it distributes the file to the Greenplum Database hosts.

-   Save the current PL/Container configuration to a file. This example saves the file to the local file `/home/gpadmin/saved_plc_config.xml`

    ```
    plcontainer runtime-backup -f /home/gpadmin/saved_plc_config.xml
    ```

-   Overwrite PL/Container configuration file with an XML file. This example replaces the information in the configuration file with the information from the file in the `/home/gpadmin` directory.

    ```
    plcontainer runtime-restore -f /home/gpadmin/new_plcontainer_configuration.xml
    ```

    The utility displays progress information on the command line as it distributes the updated file to the Greenplum Database instances.


