# plcontainer Configuration File 

The Greenplum Database utility `plcontainer` manages the PL/Container configuration files in a Greenplum Database system. The utility ensures that the configuration files are consistent across the Greenplum Database coordinator and segment instances.

> **Caution** Modifying the configuration files on the segment instances without using the utility might create different, incompatible configurations on different Greenplum Database segments that could cause unexpected behavior.

## <a id="topic_ojn_r2s_dw"></a>PL/Container Configuration File 

PL/Container maintains a configuration file `plcontainer_configuration.xml` in the data directory of all Greenplum Database segments. This query lists the Greenplum Database system data directories:

```
SELECT hostname, datadir FROM gp_segment_configuration;
```

A sample PL/Container configuration file is in `$GPHOME/share/postgresql/plcontainer`.

In an XML file, names, such as element and attribute names, and values are case sensitive.

In this XML file, the root element `configuration` contains one or more `runtime` elements. You specify the `id` of the `runtime` element in the `# container:` line of a PL/Container function definition.

This is an example file. Note that all XML elements, names, and attributes are case sensitive.

```
<?xml version="1.0" ?>
<configuration>
    <runtime>
        <id>plc_python_example1</id>
        <image>pivotaldata/plcontainer_python_with_clients:0.1</image>
        <command>./pyclient</command>
    </runtime>
    <runtime>
        <id>plc_python_example2</id>
        <image>pivotaldata/plcontainer_python_without_clients:0.1</image>
        <command>/clientdir/pyclient.sh</command>
        <shared_directory access="ro" container="/clientdir" host="/usr/local/greenplum-db/bin/plcontainer_clients"/>
        <setting memory_mb="512"/>
        <setting use_container_logging="yes"/>
        <setting cpu_share="1024"/>
        <setting resource_group_id="16391"/>
    </runtime>
    <runtime>
        <id>plc_r_example</id>
        <image>pivotaldata/plcontainer_r_without_clients:0.2</image>
        <command>/clientdir/rclient.sh</command>
        <shared_directory access="ro" container="/clientdir" host="/usr/local/greenplum-db/bin/plcontainer_clients"/>
        <setting use_container_logging="yes"/>
        <setting enable_network="no"/>
        <setting roles="gpadmin,user1"/>
    </runtime>
    <runtime>
</configuration>
```

These are the XML elements and attributes in a PL/Container configuration file.

configuration
:   Root element for the XML file.

runtime
:   One element for each specific container available in the system. These are child elements of the `configuration` element.

id
:   Required. The value is used to reference a Docker container from a PL/Container user-defined function. The `id` value must be unique in the configuration. The `id` must start with a character or digit \(a-z, A-Z, or 0-9\) and can contain characters, digits, or the characters `_` \(underscore\), `.` \(period\), or `-` \(dash\). Maximum length is 63 Bytes.

:    The `id` specifies which Docker image to use when PL/Container creates a Docker container to run a user-defined function.

image
:   Required. The value is the full Docker image name, including image tag. The same way you specify them for starting this container in Docker. Configuration allows to have many container objects referencing the same image name, this way in Docker they would be represented by identical containers.

    For example, you might have two `runtime` elements, with different `id` elements, `plc_python_128` and `plc_python_256`, both referencing the Docker image `pivotaldata/plcontainer_python:1.0.0`. The first `runtime` specifies a 128MB RAM limit and the second one specifies a 256MB limit that is specified by the `memory_mb` attribute of a `setting` element.

command
:   Required. The value is the command to be run inside of container to start the client process inside in the container. When creating a `runtime` element, the `plcontainer` utility adds a `command` element based on the language \(the `-l` option\).

:   `command` element for the Python 2 language.

    ```
    <command>/clientdir/pyclient.sh</command>
    ```

:   `command` element for the Python 3 language.

    ```
    <command>/clientdir/pyclient3.sh</command>
    ```

:   `command` element for the R language.

    ```
    <command>/clientdir/rclient.sh</command>
    ```

:   You should modify the value only if you build a custom container and want to implement some additional initialization logic before the container starts.

    > **Note** This element cannot be set with the `plcontainer` utility. You can update the configuration file with the `plcontainer runtime-edit` command.

shared\_directory
:   Optional. This element specifies a shared Docker shared volume for a container with access information. Multiple `shared_directory` elements are allowed. Each `shared_directory` element specifies a single shared volume. XML attributes for the `shared_directory` element:

    -   `host` - a directory location on the host system.
    -   `container` - a directory location inside of container.
    -   `access` - access level to the host directory, which can be either `ro` \(read-only\) or `rw` \(read-write\).

:   When creating a `runtime` element, the `plcontainer` utility adds a `shared_directory` element.

    ```
    <shared_directory access="ro" container="/clientdir" host="/usr/local/greenplum-db/bin/plcontainer_clients"/>
    ```

:   For each `runtime` element, the `container` attribute of the `shared_directory` elements must be unique. For example, a `runtime` element cannot have two `shared_directory` elements with attribute `container="/clientdir"`.

    > **Caution** Allowing read-write access to a host directory requires special consideration.

    -   When specifying read-write access to host directory, ensure that the specified host directory has the correct permissions.
    -   When running PL/Container user-defined functions, multiple concurrent Docker containers that are running on a host could change data in the host directory. Ensure that the functions support multiple concurrent access to the data in the host directory.

settings
:   Optional. This element specifies Docker container configuration information. Each `setting` element contains one attribute. The element attribute specifies logging, memory, or networking information. For example, this element enables logging.

    ```
    <setting use_container_logging="yes"/>
    ```

:   These are the valid attributes.

    cpu\_share
    :   Optional. Specify the CPU usage for each PL/Container container in the runtime. The value of the element is a positive integer. The default value is 1024. The value is a relative weighting of CPU usage compared to other containers.

    For example, a container with a `cpu_share` of 2048 is allocated double the CPU slice time compared with container with the default value of 1024.

    memory\_mb="size"
    :   Optional. The value specifies the amount of memory, in MB, that each container is allowed to use. Each container starts with this amount of RAM and twice the amount of swap space. The container memory consumption is limited by the host system `cgroups` configuration, which means in case of memory overcommit, the container is terminated by the system.

    resource\_group\_id="rg\_groupid"
    :   Optional. The value specifies the `groupid` of the resource group to assign to the PL/Container runtime. The resource group limits the total CPU and memory resource usage for all running containers that share this runtime configuration. You must specify the `groupid` of the resource group. If you do not assign a resource group to a PL/Container runtime configuration, its container instances are limited only by system resources. For information about managing PL/Container resources, see [About PL/Container Resource Management](../../analytics/pl_container_using.html).

    roles="list\_of\_roles"
    :   Optional. The value is a Greenplum Database role name or a comma-separated list of roles. PL/Container runs a container that uses the PL/Container runtime configuration only for the listed roles. If the attribute is not specified, any Greenplum Database role can run an instance of this container runtime configuration. For example, you create a UDF that specifies the `plcontainer` language and identifies a `# container:` runtime configuration that has the `roles` attribute set. When a role \(user\) runs the UDF, PL/Container checks the list of roles and runs the container only if the role is on the list.

    use\_container\_logging="\{yes \| no\}"
    :   Optional.  Activates or deactivates  Docker logging for the container. The attribute value `yes` enables logging. The attribute value `no` deactivates logging \(the default\).

    enable\_network="\{yes \| no\}"
    :   Optional. Available starting with PL/Container version 2.2, this attribute activates or deactivates network access for the UDF container. The attribute value `yes` enables UDFs to access the network. The attribute value `no` deactivates network access \(the default\).

    The Greenplum Database server configuration parameter [log\_min\_messages](../../ref_guide/config_params/guc-list.html) controls the PL/Container log level. The default log level is `warning`. For information about PL/Container log information, see [Notes](../../analytics/pl_container_using.html).

    By default, the PL/Container log information is sent to a system `journald` service.

## <a id="topic_v3s_qv3_kw"></a>Update the PL/Container Configuration 

You can add a `runtime` element to the PL/Container configuration file with the `plcontainer runtime-add` command. The command options specify information such as the runtime ID, Docker image, and language. You can use the `plcontainer runtime-replace` command to update an existing `runtime` element. The utility updates the configuration file on the coordinator and all segment instances.

The PL/Container configuration file can contain multiple `runtime` elements that reference the same Docker image specified by the XML element `image`. In the example configuration file, the `runtime` elements contain `id` elements named `plc_python_128` and `plc_python_256`, both referencing the Docker container `pivotaldata/plcontainer_python:1.0.0`. The first `runtime` element is defined with a 128MB RAM limit and the second one with a 256MB RAM limit.

```
<configuration>
  <runtime>
    <id>plc_python_128</id>
    <image>pivotaldata/plcontainer_python:1.0.0</image>
    <command>./client</command>
    <shared_directory access="ro" container="/clientdir" host="/usr/local/gpdb/bin/plcontainer_clients"/>
    <setting memory_mb="128"/>
  </runtime>
  <runtime>
    <id>plc_python_256</id>
    <image>pivotaldata/plcontainer_python:1.0.0</image>
    <command>./client</command>
    <shared_directory access="ro" container="/clientdir" host="/usr/local/gpdb/bin/plcontainer_clients"/>
    <setting memory_mb="256"/>
    <setting resource_group_id="16391"/>
  </runtime>
<configuration>
```

Configuration changes that are made with the utility are applied to the XML files on all Greenplum Database segments. However, PL/Container configurations of currently running sessions use the configuration that existed during session start up. To update the PL/Container configuration in a running session, run this command in the session.

```
SELECT * FROM plcontainer_refresh_config;
```

The command runs a PL/Container function that updates the session configuration on the coordinator and segment instances.

