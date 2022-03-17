---
title: Creating the Data Storage Areas 
---

Describes how to create the directory locations where Greenplum Database data is stored for each coordinator, standby, and segment instance.

**Parent topic:**[Installing and Upgrading Greenplum](install_guide.html)

## <a id="topic_wqb_1lc_wp"></a>Creating Data Storage Areas on the Coordinator and Standby Coordinator Hosts 

A data storage area is required on the Greenplum Database coordinator and standby coordinator hosts to store Greenplum Database system data such as catalog data and other system metadata.

### <a id="topic_ix1_x1n_tp"></a>To create the data directory location on the coordinator 

The data directory location on the coordinator is different than those on the segments. The coordinator does not store any user data, only the system catalog tables and system metadata are stored on the coordinator instance, therefore you do not need to designate as much storage space as on the segments.

1.  Create or choose a directory that will serve as your coordinator data storage area. This directory should have sufficient disk space for your data and be owned by the `gpadmin` user and group. For example, run the following commands as `root`:

    ```
    # mkdir -p /data/coordinator
    ```

2.  Change ownership of this directory to the `gpadmin` user. For example:

    ```
    # chown gpadmin:gpadmin /data/coordinator
    ```

3.  Using [gpssh](../utility_guide/ref/gpssh.html), create the coordinator data directory location on your standby coordinator as well. For example:

    ```
    # source /usr/local/greenplum-db/greenplum_path.sh 
    # gpssh -h smdw -e 'mkdir -p /data/coordinator'
    # gpssh -h smdw -e 'chown gpadmin:gpadmin /data/coordinator'
    ```


## <a id="topic_plx_zps_vhb"></a>Creating Data Storage Areas on Segment Hosts 

Data storage areas are required on the Greenplum Database segment hosts for primary segments. Separate storage areas are required for mirror segments.

### <a id="topic_tnb_v1n_tp"></a>To create the data directory locations on all segment hosts 

1.  On the coordinator host, log in as `root`:

    ```
    # su
    ```

2.  Create a file called `hostfile_gpssh_segonly`. This file should have only one machine configured host name for each segment host. For example, if you have three segment hosts:

    ```
    sdw1
    sdw2
    sdw3
    ```

3.  Using [gpssh](../utility_guide/ref/gpssh.html), create the primary and mirror data directory locations on all segment hosts at once using the `hostfile_gpssh_segonly` file you just created. For example:

    ```
    # source /usr/local/greenplum-db/greenplum_path.sh 
    # gpssh -f hostfile_gpssh_segonly -e 'mkdir -p /data/primary'
    # gpssh -f hostfile_gpssh_segonly -e 'mkdir -p /data/mirror'
    # gpssh -f hostfile_gpssh_segonly -e 'chown -R gpadmin /data/*'
    ```


## <a id="topic_cwj_hzb_vhb"></a>Next Steps 

-   [Validating Your Systems](validate.html)
-   [Initializing a Greenplum Database System](init_gpdb.html)

