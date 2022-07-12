# PL/Container Language 

PL/Container enables users to run Greenplum procedural language functions inside a Docker container, to avoid security risks associated with executing Python or R code on Greenplum segment hosts. This topic covers information about the architecture, installation, and setup of PL/Container:

-   [About the PL/Container Language Extension](#about_pl_container)
-   [Install PL/Container](#topic3)
-   [Upgrade PL/Container](#upgrade_plcontainer)
-   [Uninstall PL/Container](#uninstall_plcontainer)
-   [Docker References](#topic_kds_plk_rbb)

For detailed information about using PL/Container, refer to the sections:

-   [PL/Container Resource Management](#topic_resmgmt)
-   [PL/Container Functions](#topic_rh3_p3q_dw)

For reference documentation, see:

-   [plcontainer utility](../../utility_guide/admin_utilities/plcontainer.html) reference page
-   [plcontainer configuration file](../../utility_guide/admin_utilities/plcontainer-configuration.html) reference page

The PL/Container language extension is available as an open source module. For information about the module, see the README file in the GitHub repository at [https://github.com/greenplum-db/plcontainer](https://github.com/greenplum-db/plcontainer)

**Parent topic:** [Greenplum Database Reference Guide](../ref_guide.html)

## About the PL/Container Language Extension 

The Greenplum Database PL/Container language extension allows users to create and run PL/Python or PL/R user-defined functions \(UDFs\) securely, inside a Docker container. Docker provides the ability to package and run an application in a loosely isolated environment called a container. For information about Docker, see the [Docker web site](https://www.docker.com).

Running UDFs inside the Docker container ensures that:

-   The function execution process takes place in a separate environment and allows decoupling of the data processing. SQL operators such as "scan," "filter," and "project" are executed at the query executor \(QE\) side, and advanced data analysis is executed at the container side.
-   User code cannot access the OS or the file system of the local host.
-   User code cannot introduce any security risks.
-   Functions cannot connect back to the Greenplum Database if the container is started with limited or no network access.

### PL/Container Architecture 

![](../graphics/pl_container_architecture.png)

**Example of the process flow**:

Consider a query that selects table data using all available segments, and transforms the data using a PL/Container function. On the first call to a function in a segment container, the query executor on the master host starts the container on that segment host. It then contacts the running container to obtain the results. The container might respond with a Service Provider Interface \(SPI\) - a SQL query executed by the container to get some data back from the database - returning the result to the query executor.

A container running in standby mode waits on the socket and does not consume any CPU resources. PL/Container memory consumption depends on the amount of data cached in global dictionaries.

The container connection is closed by closing the Greenplum Database session that started the container, and the container shuts down.

## Install PL/Container 

**Warning:** PL/Container is compatible with Greenplum Database 5.2.0 and later. PL/Container has not been tested for compatibility with Greenplum Database 5.1.0 or 5.0.0.

This topic describes how to:

-   [Install Docker](#install_docker)
-   [Install PL/Container](#install_pl_utility)
-   [Install the PL/Container Docker images](#install_docker_images)
-   [Test the PL/Container installation](#test_installation)

The following sections describe these tasks in detail.

### Prerequisites 

-   PL/Container is supported on Tanzu Greenplum 5.2.x on Red Hat Enterprise Linux \(RHEL\) 7.x \(or later\) and CentOS 7.x \(or later\). PL/Container is not supported on RHEL/CentOS 6.x systems, because those platforms do not officially support Docker.

    **Note:** PL/Container 1.6.0 \(introduced in Greenplum 5.26\) and later versions support Docker images with Python 3 installed.

-   The minimum Linux OS kernel version supported is 3.10. To verfiy your kernel release use:

    ```
    $ uname -r
    ```


### Install Docker 

To use PL/Container you need to install Docker on all Greenplum Database host systems. These are the minimum Docker versions that must be installed on Greenplum Database hosts \(master, primary and all standby hosts\):

-   For PL/Container versions up to 1.5.0 - Docker 17.05
-   For PL/Container 1.6.0 and later - Docker 19.03

These instructions show how to set up the Docker service on CentOS 7, but RHEL 7 is a similar process. These steps install the docker package and start the Docker service as a user with sudo privileges.

1.  Ensure the user has sudo privileges or is root.
2.  Install the dependencies required for Docker:

    ```
    sudo yum install -y yum-utils device-mapper-persistent-data lvm2
    ```

3.  Add the Docker repo:

    ```
    sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
    ```

4.  Update yum cache:

    ```
    sudo yum makecache fast
    ```

5.  Install Docker:

    ```
    sudo yum -y install docker-ce
    ```

6.  Start Docker daemon:

    ```
    sudo systemctl start docker
    ```

7.  On each Greenplum Database host, the `gpadmin` user should be part of the docker group for the user to be able to manage Docker images and containers. Assign the Greenplum Database administrator `gpadmin` to the group `docker`:

    ```
    sudo usermod -aG docker gpadmin
    ```

8.  Exit the session and login again to update the privileges.
9.  Configure Docker to start when the host system starts:

    ```
    sudo systemctl enable docker.service
    ```

    ```
    sudo systemctl start docker.service
    ```

10. Run a Docker command to test the Docker installation. This command lists the currently running Docker containers.

    ```
    docker ps
    ```

11. After you install Docker on all Greenplum Database hosts, restart the Greenplum Database system to give Greenplum Database access to Docker.

    ```
    gpstop -ra
    ```


For a list of observations while using Docker and PL/Container, see the [Notes](#plc_notes) section. For a list of Docker reference documentation, see [Docker References](#topic_kds_plk_rbb).

### Docker Notes 

-   If a PL/Container Docker container exceeds the maximum allowed memory, it is terminated and an out of memory warning is displayed.
-   PL/Container does not limit the Docker base device size, the size of the Docker container. In some cases, the Docker daemon controls the base device size. For example, if the Docker storage driver is devicemapper, the Docker daemon `--storage-opt` option flag `dm.basesize` controls the base device size. The default base device size for devicemapper is 10GB. The Docker command `docker info` displays Docker system information including the storage driver. The base device size is displayed in Docker 1.12 and later. For information about Docker storage drivers, see the Docker information [Daemon storage-driver](https://docs.docker.com/engine/reference/commandline/dockerd/#daemon-storage-driver).

    When setting the Docker base device size, the size must be set on all Greenplum Database hosts.

-   *Known issue*:

    Occasionally, when PL/Container is running in a high concurrency environment, the Docker daemon hangs with log entries that indicate a memory shortage. This can happen even when the system seems to have adequate free memory.

    The issue seems to be triggered by the aggressive virtual memory requirement of the Go language \(golang\) runtime that is used by PL/Container, and the Greenplum Database Linux server kernel parameter setting for *overcommit\_memory*. The parameter is set to 2 which does not allow memory overcommit.

    A workaround that might help is to increase the amount of swap space and increase the Linux server kernel parameter overcommit\_ratio. If the issue still occurs after the changes, there might be memory shortage. You should check free memory on the system and add more RAM if needed. You can also decrease the cluster load.


### Install PL/Container 

**Note:** The PL/Container version 1.1 and later extension is installed by `gppkg` as a Greenplum Database extension, while the PL/Container version 1.0 extension is installed as a Greenplum Database language. Refer to the documentation associated with your Greenplum Database version to install PL/Container.

Install the PL/Container language extension using the `gppkg` utility.

1.  Download the "PL/Container for RHEL 7" package that applies to your Greenplum Database version, from the [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb/). PL/Container is listed under Greenplum Procedural Languages.
2.  Follow the instructions in [Verifying the Greenplum Database Software Download](/vmware/install_guide/verify_sw.html) to verify the integrity of the **Greenplum Procedural Languages PL/Container** software.
3.  As `gpadmin`, copy the PL/Container language extension package to the master host.
4.  Run the package installation command:

    ```
    $ gppkg -i plcontainer-1.6.0-rhel7-x86_64.gppkg
    ```

5.  Source the file `$GPHOME/greenplum_path.sh`:

    ```
    $ source $GPHOME/greenplum_path.sh
    ```

6.  Make sure Greenplum Database is up and running:

    ```
    $ gpstate -s
    ```

    If it is not, start it:

    ```
    $ gpstart -a
    ```

7.  Restart Greenplum Database:

    ```
    $ gpstop -ra
    ```

8.  Login into one of the available databases, for example:

    ```
    $ psql postgres
    ```

9.  Register the PL/Container extension, which installs the `plcontainer` utility:

    ```
    postgres=# CREATE EXTENSION plcontainer; 
    ```

    You need to register the utility separately on each database that might need the PL/Container functionality.


### Install PL/Container Docker Images 

Install the Docker images that PL/Container uses to create language-specific containers to run the UDFs.

The PL/Container open source module contains dockerfiles to build Docker images that can be used with PL/Container. You can build a Docker image to run PL/Python UDFs and a Docker image to run PL/R UDFs. See the dockerfiles in the GitHub repository at [https://github.com/greenplum-db/plcontainer](https://github.com/greenplum-db/plcontainer).

-   Download the `tar.gz` file that contains the Docker images from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).

    -   `plcontainer-python3-images-<version>.tar.gz`
    -   `plcontainer-python-images-<version>.tar.gz`
    -   `plcontainer-r-images-<version>.tar.gz`
    Each image supports a different language or language version:

    -   PL/Container for Python 3 - Docker image with Python 3.7 and the Python Data Science Module package installed.

        **Note:** PL/Container 1.6.0 and later supports Docker images with Python 3 installed.

    -   PL/Container for Python 2 - Docker image with Python 2.7.12 and the Python Data Science Module package installed.
    -   PL/Container for R - A Docker image with container with R-3.3.3 and the R Data Science Library package installed.
    The Data Science packages contain a set of Python modules or R functions and data sets related to data science. For information about the packages, see [Python Data Science Module Package](/vmware/install_guide/install_python_dsmod.html) and [R Data Science Library Package](/vmware/install_guide/install_r_dslib.html).

    If you require different images from the ones provided by Tanzu Greenplum, you can create custom Docker images, install the image, and add the image to the PL/Container configuration.

-   Use the `plcontainer` utility command `image-add` to install the images on all Greenplum Database hosts where `-f` indicates the location of the downloaded files:

    ```
    # Install a Python 2 based Docker image
    $ plcontainer image-add -f /home/gpadmin/plcontainer-python-image-1.6.0.tar.gz
                
    # Install a Python 3 based Docker image
    $ plcontainer image-add -f /home/gpadmin/plcontainer-python3-image-1.6.0.tar.gz
                
    # Install an R based Docker image
    $ plcontainer image-add -f /home/gpadmin/plcontainer-r-image-1.6.0.tar.gz
    
    ```

    The utility displays progress information, similar to:

    ```
    20200127:21:54:43:004607 plcontainer:mdw:gpadmin-[INFO]:-Checking whether docker is installed on all hosts...
    20200127:21:54:43:004607 plcontainer:mdw:gpadmin-[INFO]:-Distributing image file /home/gpadmin/plcontainer-python-images-1.6.0.tar to all hosts...
    20200127:21:54:55:004607 plcontainer:mdw:gpadmin-[INFO]:-Loading image on all hosts...
    20200127:21:55:37:004607 plcontainer:mdw:gpadmin-[INFO]:-Removing temporary image files on all hosts...
    ```

    For more information on `image-add` options, visit the [plcontainer](../../utility_guide/admin_utilities/plcontainer.html) reference page.

-   To display the installed Docker images on the local host use:

    ```
    $ plcontainer image-list
    ```

    |REPOSITORY|TAG|IMAGE ID|
    |----------|---|--------|
    |pivotaldata/plcontainer\_r\_shared|devel|7427f920669d|
    |pivotaldata/plcontainer\_python\_shared|devel|e36827eba53e|
    |pivotaldata/plcontainer\_python3\_shared|devel|y32827ebe55b|

-   Add the image information to the PL/Container configuration file using `plcontainer runtime-add`, to allow PL/Container to associate containers with specified Docker images.

    Use the `-r` option to specify your own user defined runtime ID name, use the `-i` option to specify the Docker image, and the `-l` option to specify the Docker image language. When there are multiple versions of the same docker image, for example 1.0.0 or 1.2.0, specify the TAG version using ":" after the image name.

    ```
    # Add a Python 2 based runtime
    $ plcontainer runtime-add -r plc_python_shared -i pivotaldata/plcontainer_python_shared:devel -l python
                
    # Add a Python 3 based runtime that is supported with PL/Container 1.6.0
    $ plcontainer runtime-add -r plc_python3_shared -i pivotaldata/plcontainer_python3_shared:devel -l python3
                
    # Add an R based runtime
    $ plcontainer runtime-add -r plc_r_shared -i pivotaldata/plcontainer_r_shared:devel -l r
    ```

    The utility displays progress information as it updates the PL/Container configuration file on the Greenplum Database instances.

    For details on other `runtime-add` options, see the [plcontainer](../../utility_guide/admin_utilities/plcontainer.html) reference page.

-   Optional: Use Greenplum Database resource groups to manage and limit the total CPU and memory resources of containers in PL/Container runtimes. In this example, the Python runtime will be used with a preconfigured resource group 16391:

    ```
    $ plcontainer runtime-add -r plc_python_shared -i pivotaldata/plcontainer_python_shared:devel -l
     python -s resource_group_id=16391
    ```

    For more information about enabling, configuring, and using Greenplum Database resource groups with PL/Container, see [PL/Container Resource Management](#topic_resmgmt) .


You can now create a simple function to test your PL/Container installation.

### Test the PL/Container Installation 

List the names of the runtimes your created and added to the PL/Container XML file:

```
$ plcontainer runtime-show
```

The command shows a list of all installed runtimes:

```
PL/Container Runtime Configuration: 
---------------------------------------------------------
  Runtime ID: plc_python_shared
  Linked Docker Image: pivotaldata/plcontainer_python_shared:devel
  Runtime Setting(s): 
  Shared Directory: 
  ---- Shared Directory From HOST '/usr/local/greenplum-db/./bin/plcontainer_clients' to Container '/clientdir', access mode is 'ro'
---------------------------------------------------------
```

You can also view the PL/Container configuration information with the `plcontainer runtime-show -r <runtime_id>` command. You can view the PL/Container configuration XML file with the `plcontainer runtime-edit` command.

Use the `psql` utility and select an existing database:

```
$ psql postgres;
```

If the PL/Container extension is not registered with the selected database, first enable it using:

```
postgres=# CREATE EXTENSION plcontainer;
```

Create a simple function to test your installation; in the example, the function will use the runtime `plc_python_shared`:

```
postgres=# CREATE FUNCTION dummyPython() RETURNS text AS $$
# container: plc_python_shared
return 'hello from Python'
$$ LANGUAGE plcontainer;
```

Test the function using:

```
postgres=# SELECT dummyPython();
    dummypython    
-------------------
 hello from Python
(1 row)

```

Similarly, to test the R runtime:

```
postgres=# CREATE FUNCTION dummyR() RETURNS text AS $$
# container: plc_r_shared
return ('hello from R')
$$ LANGUAGE plcontainer;
CREATE FUNCTION
postgres=# select dummyR();
    dummyr    
--------------
 hello from R
(1 row)
```

For further details and examples about using PL/Container functions, see [PL/Container Functions](#topic_kwg_qfg_mjb).

## Upgrade PL/Container 

### Upgrading from PL/Container 1.1 or Later 

**Note:** To upgrade PL/Container 1.0, you must uninstall the old version and install the new version. See [Upgrading from PL/Container 1.0](#section_blp_5w5_flb).

To upgrade from PL/Container 1.1 or higher, you save the current configuration, use `gppkg` to upgrade the PL/Container software, and then restore the configuration after upgrade. There is no need to update the Docker images when you upgrade PL/Container.

**Note:** Before you perform this upgrade procedure, ensure that you have migrated your existing PL/Container package from your previous Greenplum Database installation to your new Greenplum Database installation. Refer to the [gppkg](../../utility_guide/admin_utilities/gppkg.html) command for package installation and migration information.

Perform the following procedure to upgrade from PL/Container 1.1 or later:

1.  Save the PL/Container configuration. For example, to save the configuration to a file named `plcontainer150-backup.xml` in the local directory:

    ```
    $ plcontainer runtime-backup -f plcontainer150-backup.xml
    ```

2.  Use the Greenplum Database `gppkg` utility with the `-u` option to update the PL/Container language extension. For example, the following command updates the PL/Container language extension to version 1.6.0 on a Linux system:

    ```
    $ gppkg -u plcontainer-1.6.0-rhel7-x86_64.gppkg
    ```

3.  Source the Greenplum Database environment file `$GPHOME/greenplum_path.sh`:

    ```
    $ source $GPHOME/greenplum_path.sh
    ```

4.  Restore the PL/Container configuration. For example, this command restores the PL/Container configuration that you saved in a previous step:

    ```
    $ plcontainer runtime-restore -f plcontainer150-backup.xml
    ```

5.  Restart Greenplum Database:

    ```
    $ gpstop -ra
    ```

6.  You do not need to re-register the PL/Container extension in the databases in which you previously created the extension. However, you must register the PL/Container extension in each new database that will run PL/Container UDFs. For example, the following command registers PL/Container in a database named `mytest`:

    ```
    $ psql -d mytest -c 'CREATE EXTENSION plcontainer;'
    ```

    The command also creates PL/Container-specific functions and views.


**Note:** PL/Container 1.2 and later utilizes the new resource group capabilities introduced in Greenplum Database 5.8.0. If you downgrade to a Greenplum Database system that uses PL/Container 1.1. or earlier, you must use `plcontainer runtime-edit` to remove any `resource_group_id` settings from your PL/Container runtime configuration.

### Upgrading from PL/Container 1.0 

You cannot use the `gppkg -u` option to upgrade from PL/Container 1.0, because PL/Container 1.0 is installed as a Greenplum Database language rather than as an extension. To upgrade to from PL/Container 1.0, you must install uninstall version 1.0 and then install the new version. The Docker images and the PL/Container configuration do not change when upgrading PL/Container, only the PL/Container extension installation changes.

As part of the upgrade process, you must drop PL/Container from all databases that are configured with PL/Container.

**Important:** Dropping PL/Container from a database drops all PL/Container UDFs from the database, including user-created PL/Container UDFs. If the UDFs are required, ensure you can re-create the UDFs before dropping PL/Container. This `SELECT` command lists the names of and body of PL/Container UDFs in a database.

```
SELECT proname, prosrc FROM pg_proc WHERE prolang = (SELECT oid FROM pg_language WHERE lanname = 'plcontainer');
```

For information about the catalog tables, `pg_proc` and `pg_language`, see [System Tables](../system_catalogs/catalog_ref-tables.html).

These steps upgrade from PL/Container 1.0 to PL/Container 1.1 or later. The steps save the PL/Container 1.0 configuration and restore the configuration for use with PL/Container 1.1 or later.

1.  Save the PL/Container configuration. This example saves the configuration to `plcontainer10-backup.xml` in the local directory.

    ```
    $ plcontainer runtime-backup -f plcontainer10-backup.xml
    ```

2.  Remove any `setting` elements that contain the `use_container_network` attribute from the configuration file. For example, this `setting` element must be removed from the configuration file.

    ```
    <setting use_container_network="yes"/>
    ```

3.  Run the `plcontainer_uninstall.sql` script as the `gpadmin` user for each database that is configured with PL/Container. For example, this command drops the `plcontainer` language in the `mytest` database.

    ```
    $ psql -d mytest -f $GPHOME/share/postgresql/plcontainer/plcontainer_uninstall.sql
    ```

    The script drops the `plcontainer` language with the `CASCADE` clause that drops PL/Container-specific functions and views in the database.

4.  Use the Greenplum Database `gppkg` utility with the `-r` option to uninstall the PL/Container language extension. This example uninstalls the PL/Container language extension on a Linux system.

    ```
    $ gppkg -r plcontainer-1.0.0
    ```

5.  Run the package installation command. This example installs the PL/Container 1.6.0 language extension on a Linux system.

    ```
    $ gppkg -i plcontainer-1.6.0-rhel7-x86_64.gppkg
    ```

6.  Source the file `$GPHOME/greenplum_path.sh`.

    ```
    $ source $GPHOME/greenplum_path.sh
    ```

7.  Update the PL/Container configuration. This command restores the saved configuration.

    ```
    $ plcontainer runtime-restore -f plcontainer10-backup.xml
    ```

8.  Restart Greenplum Database.

    ```
    $ gpstop -ra
    ```

9.  Register the new PL/Container extension as an extension for each database that uses PL/Container UDFs. This `psql` command runs a `CREATE EXTENSION` command to register PL/Container in the database `mytest`.

    ```
    $ psql -d mytest -c 'CREATE EXTENSION plcontainer;'
    ```

    The command registers PL/Container as an extension and creates PL/Container-specific functions and views.


After upgrading PL/Container for a database, re-install any user-created PL/Container UDFs that are required.

## Uninstall PL/Container 

To uninstall PL/Container, remove Docker containers and images, and then remove the PL/Container support from Greenplum Database.

When you remove support for PL/Container, the `plcontainer` user-defined functions that you created in the database will no longer work.

### Uninstall Docker Containers and Images 

On the Greenplum Database hosts, uninstall the Docker containers and images that are no longer required.

The `plcontainer image-list` command lists the Docker images that are installed on the local Greenplum Database host.

The `plcontainer image-delete` command deletes a specified Docker image from all Greenplum Database hosts.

Some Docker containers might exist on a host if the containers were not managed by PL/Container. You might need to remove the containers with Docker commands. These `docker` commands manage Docker containers and images on a local host.

-   The command `docker ps -a` lists all containers on a host. The command `docker stop` stops a container.
-   The command `docker images` lists the images on a host.
-   The command `docker rmi` removes images.
-   The command `docker rm` removes containers.

### Remove PL/Container Support for a Database 

To remove support for PL/Container, drop the extension from the database. Use the `psql` utility with `DROP EXTENION` command \(using `-c`\) to remove PL/Container from `mytest` database.

```
psql -d mytest -c 'DROP EXTENSION plcontainer CASCADE;'
```

The `CASCADE` keyword drops PL/Container-specific functions and views.

### Uninstall the PL/Container Language Extension 

If no databases have `plcontainer` as a registered language, uninstall the Greenplum Database PL/Container language extension with the `gppkg` utility.

1.  Use the Greenplum Database `gppkg` utility with the `-r` option to uninstall the PL/Container language extension. This example uninstalls the PL/Container language extension on a Linux system:

    ```
    $ gppkg -r plcontainer-2.1.1
    ```

    You can run the `gppkg` utility with the options `-q --all` to list the installed extensions and their versions.

2.  Reload `greenplum_path.sh`.

    ```
    $ source $GPHOME/greenplum_path.sh
    ```

3.  Restart the database.

    ```
    $ gpstop -ra
    ```


## Using PL/Container 

This topic covers further details on:

-   [PL/Container Resource Management](#topic_resmgmt)
-   [PL/Container Functions](#topic_rh3_p3q_dw)

## PL/Container Resource Management 

The Docker containers and the Greenplum Database servers share CPU and memory resources on the same hosts. In the default case, Greenplum Database is unaware of the resources consumed by running PL/Container instances. You can use Greenplum Database resource groups to control overall CPU and memory resource usage for running PL/Container instances.

PL/Container manages resource usage at two levels - the container level and the runtime level. You can control container-level CPU and memory resources with the `memory_mb` and `cpu_share` settings that you configure for the PL/Container runtime. `memory_mb` governs the memory resources available to each container instance. The `cpu_share` setting identifies the relative weighting of a container's CPU usage compared to other containers. See [../../utility\_guide/admin\_utilities/plcontainer-configuration.md](../../utility_guide/admin_utilities/plcontainer-configuration.html) for further details.

You cannot, by default, restrict the number of executing PL/Container container instances, nor can you restrict the total amount of memory or CPU resources that they consume.

### Using Resource Groups to Manage PL/Container Resources 

With PL/Container 1.2.0 and later, you can use Greenplum Database resource groups to manage and limit the total CPU and memory resources of containers in PL/Container runtimes. For more information about enabling, configuring, and using Greenplum Database resource groups, refer to [Using Resource Groups](../../admin_guide/workload_mgmt_resgroups.html) in the *Greenplum Database Administrator Guide*.

**Note:** If you do not explicitly configure resource groups for a PL/Container runtime, its container instances are limited only by system resources. The containers may consume resources at the expense of the Greenplum Database server.

Resource groups for external components such as PL/Container use Linux control groups \(cgroups\) to manage component-level use of memory and CPU resources. When you manage PL/Container resources with resource groups, you configure both a memory limit and a CPU limit that Greenplum Database applies to all container instances that share the same PL/Container runtime configuration.

When you create a resource group to manage the resources of a PL/Container runtime, you must specify `MEMORY_AUDITOR=cgroup` and `CONCURRENCY=0` in addition to the required CPU and memory limits. For example, the following command creates a resource group named `plpy_run1_rg` for a PL/Container runtime:

```
CREATE RESOURCE GROUP plpy_run1_rg WITH (MEMORY_AUDITOR=cgroup, CONCURRENCY=0,
          CPU_RATE_LIMIT=10, MEMORY_LIMIT=10);
```

PL/Container does not use the `MEMORY_SHARED_QUOTA` and `MEMORY_SPILL_RATIO` resource group memory limits. Refer to the [CREATE RESOURCE GROUP](../../ref_guide/sql_commands/CREATE_RESOURCE_GROUP.html) reference page for detailed information about this SQL command.

You can create one or more resource groups to manage your running PL/Container instances. After you create a resource group for PL/Container, you assign the resource group to one or more PL/Container runtimes. You make this assignment using the `groupid` of the resource group. You can determine the `groupid` for a given resource group name from the `gp_resgroup_config` `gp_toolkit` view. For example, the following query displays the `groupid` of a resource group named `plpy_run1_rg`:

```
SELECT groupname, groupid FROM gp_toolkit.gp_resgroup_config
 WHERE groupname='plpy_run1_rg';
                            
 groupname   |  groupid
 --------------+----------
 plpy_run1_rg |   16391
 (1 row)
```

You assign a resource group to a PL/Container runtime configuration by specifying the `-s resource_group_id=rg\_groupid` option to the `plcontainer runtime-add` \(new runtime\) or `plcontainer runtime-replace` \(existing runtime\) commands. For example, to assign the `plpy_run1_rg` resource group to a new PL/Container runtime named `python_run1`:

```
plcontainer runtime-add -r python_run1 -i pivotaldata/plcontainer_python_shared:devel -l python -s resource_group_id=16391
```

You can also assign a resource group to a PL/Container runtime using the `plcontainer runtime-edit` command. For information about the `plcontainer` command, see [../../utility\_guide/admin\_utilities/plcontainer.md](../../utility_guide/admin_utilities/plcontainer.html) reference page.

After you assign a resource group to a PL/Container runtime, all container instances that share the same runtime configuration are subject to the memory limit and the CPU limit that you configured for the group. If you decrease the memory limit of a PL/Container resource group, queries executing in running containers in the group may fail with an out of memory error. If you drop a PL/Container resource group while there are running container instances, Greenplum Database kills the running containers.

### Configuring Resource Groups for PL/Container 

To use Greenplum Database resource groups to manage PL/Container resources, you must explicitly configure both resource groups and PL/Container.

Perform the following procedure to configure PL/Container to use Greenplum Database resource groups for CPU and memory resource management:

1.  If you have not already configured and enabled resource groups in your Greenplum Database deployment, configure cgroups and enable Greenplum Database resource groups as described in [Using Resource Groups](../../admin_guide/workload_mgmt_resgroups.html) in the *Greenplum Database Administrator Guide*.

    **Note:** If you have previously configured and enabled resource groups in your deployment, ensure that the Greenplum Database resource group `gpdb.conf` cgroups configuration file includes a `memory { }` block as described in the previous link.

2.  Analyze the resource usage of your Greenplum Database deployment. Determine the percentage of resource group CPU and memory resources that you want to allocate to PL/Container Docker containers.
3.  Determine how you want to distribute the total PL/Container CPU and memory resources that you identified in the step above among the PL/Container runtimes. Identify:
    -   The number of PL/Container resource group\(s\) that you require.
    -   The percentage of memory and CPU resources to allocate to each resource group.
    -   The resource-group-to-PL/Container-runtime assignment\(s\).
4.  Create the PL/Container resource groups that you identified in the step above. For example, suppose that you choose to allocate 25% of both memory and CPU Greenplum Database resources to PL/Container. If you further split these resources among 2 resource groups 60/40, the following SQL commands create the resource groups:

    ```
    CREATE RESOURCE GROUP plr_run1_rg WITH (MEMORY_AUDITOR=cgroup, CONCURRENCY=0,
                                       CPU_RATE_LIMIT=15, MEMORY_LIMIT=15);
     CREATE RESOURCE GROUP plpy_run1_rg WITH (MEMORY_AUDITOR=cgroup, CONCURRENCY=0,
                                       CPU_RATE_LIMIT=10, MEMORY_LIMIT=10);
    ```

5.  Find and note the `groupid` associated with each resource group that you created. For example:

    ```
    SELECT groupname, groupid FROM gp_toolkit.gp_resgroup_config
    WHERE groupname IN ('plpy_run1_rg', 'plr_run1_rg');
                                        
    groupname   |  groupid
    --------------+----------
    plpy_run1_rg |   16391
    plr_run1_rg  |   16393
    (1 row)
    ```

6.  Assign each resource group that you created to the desired PL/Container runtime configuration. If you have not yet created the runtime configuration, use the `plcontainer runtime-add` command. If the runtime already exists, use the `plcontainer runtime-replace` or `plcontainer runtime-edit` command to add the resource group assignment to the runtime configuration. For example:

    ```
    plcontainer runtime-add -r python_run1 -i pivotaldata/plcontainer_python_shared:devel -l python -s resource_group_id=16391
    plcontainer runtime-replace -r r_run1 -i pivotaldata/plcontainer_r_shared:devel -l r -s resource_group_id=16393
    ```

    For information about the `plcontainer` command, see [../../utility\_guide/admin\_utilities/plcontainer.md](../../utility_guide/admin_utilities/plcontainer.html) reference page.


### Notes 

**PL/Container logging**

When PL/Container logging is enabled, you can set the log level with the Greenplum Database server configuration parameter [log\_min\_messages](../../ref_guide/config_params/guc-list.html). The default log level is `warning`. The parameter controls the PL/Container log level and also controls the Greenplum Database log level.

-   PL/Container logging is enabled or disabled for each runtime ID with the `setting` attribute `use_container_logging`. The default is no logging.
-   The PL/Container log information is the information from the UDF that is run in the Docker container. By default, the PL/Container log information is sent to a system service. On Red Hat 7 or CentOS 7 systems, the log information is sent to the `journald` service.
-   The Greenplum Database log information is sent to log file on the Greenplum Database master.
-   When testing or troubleshooting a PL/Container UDF, you can change the Greenplum Database log level with the `SET` command. You can set the parameter in the session before you run your PL/Container UDF. This example sets the log level to `debug1`.

    ```
    SET log_min_messages='debug1' ;
    ```

    **Note:** The parameter `log_min_messages` controls both the Greenplum Database and PL/Container logging, increasing the log level might affect Greenplum Database performance even if a PL/Container UDF is not running.


## PL/Container Functions 

When you enable PL/Container in a database of a Greenplum Database system, the language `plcontainer` is registered in that database. Specify `plcontainer` as a language in a UDF definition to create and run user-defined functions in the procedural languages supported by the PL/Container Docker images.

### Limitations 

Review the following limitations when creating and using PL/Container PL/Python and PL/R functions:

-   Greenplum Database domains are not supported.
-   Multi-dimensional arrays are not supported.
-   Python and R call stack information is not displayed when debugging a UDF.
-   The `plpy.execute()` methods `nrows()` and `status()` are not supported.
-   The PL/Python function `plpy.SPIError()` is not supported.
-   Executing the `SAVEPOINT` command with `plpy.execute()` is not supported.
-   The `DO` command is not supported.
-   Container flow control is not supported.
-   Triggers are not supported.
-   `OUT` parameters are not supported.
-   The Python `dict` type cannot be returned from a PL/Python UDF. When returning the Python `dict` type from a UDF, you can convert the `dict` type to a Greenplum Database user-defined data type \(UDT\).

### Using PL/Container functions 

A UDF definition that uses PL/Container must have the these items.

-   The first line of the UDF must be `# container: ID`
-   The `LANGUAGE` attribute must be `plcontainer`

The ID is the name that PL/Container uses to identify a Docker image. When Greenplum Database executes a UDF on a host, the Docker image on the host is used to start a Docker container that runs the UDF. In the XML configuration file `plcontainer_configuration.xml`, there is a `runtime` XML element that contains a corresponding `id` XML element that specifies the Docker container startup information. See [../../utility\_guide/admin\_utilities/plcontainer-configuration.md\#](../../utility_guide/admin_utilities/plcontainer-configuration.html) for information about how PL/Container maps the ID to a Docker image. See [\#function\_examples](#function_examples) for example UDF definitions.

The PL/Container configuration file is read only on the first invocation of a PL/Container function in each Greenplum Database session that runs PL/Container functions. You can force the configuration file to be re-read by performing a `SELECT` command on the view `plcontainer_refresh_config` during the session. For example, this `SELECT` command forces the configuration file to be read.

```
SELECT * FROM plcontainer_refresh_config;
```

Running the command executes a PL/Container function that updates the configuration on the master and segment instances and returns the status of the refresh.

```
 gp_segment_id | plcontainer_refresh_local_config
 ---------------+----------------------------------
 1 | ok
 0 | ok
-1 | ok
(3 rows)
```

Also, you can show all the configurations in the session by performing a `SELECT` command on the view `plcontainer_show_config`. For example, this `SELECT` command returns the PL/Container configurations.

```
SELECT * FROM plcontainer_show_config;
```

Running the command executes a PL/Container function that displays configuration information from the master and segment instances. This is an example of the start and end of the view output.

```
INFO:  plcontainer: Container 'plc_py_test' configuration
 INFO:  plcontainer:     image = 'pivotaldata/plcontainer_python_shared:devel'
 INFO:  plcontainer:     memory_mb = '1024'
 INFO:  plcontainer:     use container network = 'no'
 INFO:  plcontainer:     use container logging  = 'no'
 INFO:  plcontainer:     shared directory from host '/usr/local/greenplum-db/./bin/plcontainer_clients' to container '/clientdir'
 INFO:  plcontainer:     access = readonly
                
 ...
                
 INFO:  plcontainer: Container 'plc_r_example' configuration  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     image = 'pivotaldata/plcontainer_r_without_clients:0.2'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     memory_mb = '1024'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     use container network = 'no'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     use container logging  = 'yes'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:     shared directory from host '/usr/local/greenplum-db/bin/plcontainer_clients' to container '/clientdir'  (seg0 slice3 192.168.180.45:40000 pid=3304)
 INFO:  plcontainer:         access = readonly  (seg0 slice3 192.168.180.45:40000 pid=3304)
 gp_segment_id | plcontainer_show_local_config
 ---------------+-------------------------------
  0 | ok
 -1 | ok
  1 | ok
```

The PL/Container function `plcontainer_containers_summary()` displays information about the currently running Docker containers.

```
SELECT * FROM plcontainer_containers_summary();
```

If a normal \(non-superuser\) Greenplum Database user runs the function, the function displays information only for containers created by the user. If a Greenplum Database superuser runs the function, information for all containers created by Greenplum Database users is displayed. This is sample output when 2 containers are running.

```
 SEGMENT_ID |                           CONTAINER_ID                           |   UP_TIME    |  OWNER  | MEMORY_USAGE(KB)
 ------------+------------------------------------------------------------------+--------------+---------+------------------
 1          | 693a6cb691f1d2881ec0160a44dae2547a0d5b799875d4ec106c09c97da422ea | Up 8 seconds | gpadmin | 12940
 1          | bc9a0c04019c266f6d8269ffe35769d118bfb96ec634549b2b1bd2401ea20158 | Up 2 minutes | gpadmin | 13628
 (2 rows)
```

When Greenplum Database executes a PL/Container UDF, Query Executer \(QE\) processes start Docker containers and reuse them as needed. After a certain amount of idle time, a QE process quits and destroys its Docker containers. You can control the amount of idle time with the Greenplum Database server configuration parameter [gp\_vmem\_idle\_resource\_timeout](../../ref_guide/config_params/guc-list.html). Controlling the idle time might help with Docker container reuse and avoid the overhead of creating and starting a Docker container.

**Warning:** Changing `gp_vmem_idle_resource_timeout` value, might affect performance due to resource issues. The parameter also controls the freeing of Greenplum Database resources other than Docker containers.

### Examples 

The values in the `# container` lines of the examples, `plc_python_shared` and `plc_r_shared`, are the `id` XML elements defined in the `plcontainer_config.xml` file. The `id` element is mapped to the `image` element that specifies the Docker image to be started. If you configured PL/Container with a different ID, change the value of the `# container` line. For information about configuring PL/Container and viewing the configuration settings, see [../../utility\_guide/admin\_utilities/plcontainer-configuration.md](../../utility_guide/admin_utilities/plcontainer-configuration.html).

This is an example of PL/Python function that runs using the `plc_python_shared` container that contains Python 2:

```
CREATE OR REPLACE FUNCTION pylog100() RETURNS double precision AS $$
 # container: plc_python_shared
 import math
 return math.log10(100)
 $$ LANGUAGE plcontainer;
```

This is an example of a similar function using the `plc_r_shared` container:

```
CREATE OR REPLACE FUNCTION rlog100() RETURNS text AS $$
# container: plc_r_shared
return(log10(100))
$$ LANGUAGE plcontainer;
```

If the `# container` line in a UDF specifies an ID that is not in the PL/Container configuration file, Greenplum Database returns an error when you try to execute the UDF.

### About PL/Container Running PL/Python 

In the Python language container, the module `plpy` is implemented. The module contains these methods:

-   `plpy.execute(stmt)` - Executes the query string `stmt` and returns query result in a list of dictionary objects. To be able to access the result fields ensure your query returns named fields.
-   `plpy.prepare(stmt[, argtypes])` - Prepares the execution plan for a query. It is called with a query string and a list of parameter types, if you have parameter references in the query.
-   `plpy.execute(plan[, argtypes])` - Executes a prepared plan.
-   `plpy.debug(msg)` - Sends a DEBUG2 message to the Greenplum Database log.
-   `plpy.log(msg)` - Sends a LOG message to the Greenplum Database log.
-   `plpy.info(msg)` - Sends an INFO message to the Greenplum Database log.
-   `plpy.notice(msg)` - Sends a NOTICE message to the Greenplum Database log.
-   `plpy.warning(msg)` - Sends a WARNING message to the Greenplum Database log.
-   `plpy.error(msg)` - Sends an ERROR message to the Greenplum Database log. An ERROR message raised in Greenplum Database causes the query execution process to stop and the transaction to rollback.
-   `plpy.fatal(msg)` - Sends a FATAL message to the Greenplum Database log. A FATAL message causes Greenplum Database session to be closed and transaction to be rolled back.
-   `plpy.subtransaction()` - Manages `plpy.execute` calls in an explicit subtransaction. See [Explicit Subtransactions](https://www.postgresql.org/docs/9.4/plpython-subtransaction.html) in the PostgreSQL documentation for additional information about `plpy.subtransaction()`.

If an error of level `ERROR` or `FATAL` is raised in a nested Python function call, the message includes the list of enclosing functions.

The Python language container supports these string quoting functions that are useful when constructing ad-hoc queries.

-   `plpy.quote_literal(string)` - Returns the string quoted to be used as a string literal in an SQL statement string. Embedded single-quotes and backslashes are properly doubled. `quote_literal()` returns null on null input \(empty input\). If the argument might be null, `quote_nullable()` might be more appropriate.
-   `plpy.quote_nullable(string)` - Returns the string quoted to be used as a string literal in an SQL statement string. If the argument is null, returns `NULL`. Embedded single-quotes and backslashes are properly doubled.
-   `plpy.quote_ident(string)` - Returns the string quoted to be used as an identifier in an SQL statement string. Quotes are added only if necessary \(for example, if the string contains non-identifier characters or would be case-folded\). Embedded quotes are properly doubled.

When returning text from a PL/Python function, PL/Container converts a Python unicode object to text in the database encoding. If the conversion cannot be performed, an error is returned.

PL/Container does not support this Greenplum Database PL/Python feature:

-   Multi-dimensional arrays.

Also, the Python module has two global dictionary objects that retain the data between function calls. They are named GD and SD. GD is used to share the data between all the function running within the same container, while SD is used for sharing the data between multiple calls of each separate function. Be aware that accessing the data is possible only within the same session, when the container process lives on a segment or master. Be aware that for idle sessions Greenplum Database terminates segment processes, which means the related containers would be shut down and the data from GD and SD lost.

For information about PL/Python, see [Greenplum PL/Python Language Extension](pl_python.html).

For information about the `plpy` methods, see [https://www.postgresql.org/docs/9.4/plpython-database.htm](https://www.postgresql.org/docs/9.4/plpython-database.html).

### About PL/Container Running PL/Python with Python 3 

PL/Container for Greenplum Database 5 supports Python version 3.6+. PL/Container for Greenplum Database 6 supports Python 3.7+.

If you want to use PL/Container to run the same function body in both Python2 and Python3, you must create 2 different user-defined functions.

Keep in mind that UDFs that you created for Python 2 may not run in PL/Container with Python 3. The following Python references may be useful:

-   Changes to Python - [What’s New in Python 3](https://docs.python.org/3/whatsnew/3.0.html)
-   Porting from Python 2 to 3 - [Porting Python 2 Code to Python 3](https://docs.python.org/3/howto/pyporting.html)

### About PL/Container Running PL/R 

In the R language container, the module `pg.spi` is implemented. The module contains these methods:

-   `pg.spi.exec(stmt)` - Executes the query string `stmt` and returns query result in R `data.frame`. To be able to access the result fields make sure your query returns named fields.
-   `pg.spi.prepare(stmt[, argtypes])` - Prepares the execution plan for a query. It is called with a query string and a list of parameter types if you have parameter references in the query.
-   `pg.spi.execp(plan[, argtypes])` - Execute a prepared plan.
-   `pg.spi.debug(msg)` - Sends a DEBUG2 message to the Greenplum Database log.
-   `pg.spi.log(msg)` - Sends a LOG message to the Greenplum Database log.
-   `pg.spi.info(msg)` - Sends an INFO message to the Greenplum Database log.
-   `pg.spi.notice(msg)` - Sends a NOTICE message to the Greenplum Database log.
-   `pg.spi.warning(msg)` - Sends a WARNING message to the Greenplum Database log.
-   `pg.spi.error(msg)` - Sends an ERROR message to the Greenplum Database log. An ERROR message raised in Greenplum Database causes the query execution process to stop and the transaction to rollback.
-   `pg.spi.fatal(msg)` - Sends a FATAL message to the Greenplum Database log. A FATAL message causes Greenplum Database session to be closed and transaction to be rolled back.

PL/Container does not support this PL/R feature:

-   Multi-dimensional arrays.

For information about PL/R, see [Greenplum PL/R Language Extension](pl_r.html).

For information about the `pg.spi` methods, see [http://www.joeconway.com/plr/doc/plr-spi-rsupport-funcs-normal.html](http://www.joeconway.com/plr/doc/plr-spi-rsupport-funcs-normal.html)

## Docker References 

Docker home page [https://www.docker.com/](https://www.docker.com/)

Docker command line interface [https://docs.docker.com/engine/reference/commandline/cli/](https://docs.docker.com/engine/reference/commandline/cli/)

Dockerfile reference [https://docs.docker.com/engine/reference/builder/](https://docs.docker.com/engine/reference/builder/)

For CentOS, see [Docker site installation instructions for CentOS](https://docs.docker.com/engine/installation/linux/centos/).

For a list of Docker commands, see the [Docker engine Run Reference](https://docs.docker.com/engine/reference/run/).

Installing Docker on Linux systems [https://docs.docker.com/engine/installation/linux/centos/](https://docs.docker.com/engine/installation/linux/centos/)

Control and configure Docker with systemd [https://docs.docker.com/engine/admin/systemd/](https://docs.docker.com/engine/admin/systemd/)

