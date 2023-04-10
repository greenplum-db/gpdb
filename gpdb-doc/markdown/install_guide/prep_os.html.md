---
title: Configuring Your Systems 
---

Describes how to prepare your operating system environment for Greenplum Database software installation.

Perform the following tasks in order:

1.  Make sure your host systems meet the requirements described in [On-Premise Hardware Requirements](./platform-requirements-overview.html#on-prem).
2.  [Deactivate or configure SELinux.](#topic_sqj_lt1_nfb)
3.  [Deactivate or configure firewall software.](#topic_et2_y22_4nb)
4.  [Set the required operating system parameters.](#topic3)
5.  [Synchronize system clocks.](#topic_qst_s5t_wy)
6.  [Create the gpadmin account.](#topic23)

Unless noted, these tasks should be performed for *all* hosts in your Greenplum Database array \(coordinator, standby coordinator, and segment hosts\).

The Greenplum Database host naming convention for the coordinator host is `cdw` and for the standby coordinator host is `scdw`.

The segment host naming convention is sdwN where sdw is a prefix and N is an integer. For example, segment host names would be `sdw1`, `sdw2` and so on. NIC bonding is recommended for hosts with multiple interfaces, but when the interfaces are not bonded, the convention is to append a dash \(`-`\) and number to the host name. For example, `sdw1-1` and `sdw1-2` are the two interface names for host `sdw1`.

For information about running VMware Greenplum Database in the cloud see *Cloud Services* in the [VMware Greenplum Partner Marketplace](https://pivotal.io/pivotal-greenplum/greenplum-partner-marketplace).

> **Important** When data loss is not acceptable for a Greenplum Database cluster, Greenplum coordinator and segment mirroring is recommended. If mirroring is not enabled then Greenplum stores only one copy of the data, so the underlying storage media provides the only guarantee for data availability and correctness in the event of a hardware failure.

The VMware Greenplum on vSphere virtualized environment ensures the enforcement of anti-affinity rules required for Greenplum mirroring solutions and fully supports mirrorless deployments. Other virtualized or containerized deployment environments are generally not supported for production use unless both Greenplum coordinator and segment mirroring are enabled.

> **Note** For information about upgrading VMware Greenplum from a previous version, see the *VMware Greenplum Database Release Notes* for the release that you are installing.

> **Note** Automating the configuration steps described in this topic and [Installing the Greenplum Database Software](install_gpdb.html) with a system provisioning tool, such as Ansible, Chef, or Puppet, can save time and ensure a reliable and repeatable Greenplum Database installation.

**Parent topic:** [Installing and Upgrading Greenplum](install_guide.html)

## <a id="topic_sqj_lt1_nfb"></a>Deactivate or Configure SELinux 

For all Greenplum Database host systems running RHEL/Oracle/Rocky Linux, SELinux must either be `Disabled` or configured to allow unconfined access to Greenplum processes, directories, and the gpadmin user.

If you choose to deactivate SELinux:

1.  As the root user, check the status of SELinux:

    ```
    # sestatus
    SELinuxstatus: disabled
    ```

2.  If SELinux is not deactivated, deactivate it by editing the `/etc/selinux/config` file. As root, change the value of the `SELINUX` parameter in the `config` file as follows:

    ```
    SELINUX=disabled
    ```

3.  If the System Security Services Daemon \(SSSD\) is installed on your systems, edit the SSSD configuration file and set the `selinux_provider` parameter to `none` to prevent SELinux-related SSH authentication denials that could occur even with SELinux deactivated. As root, edit `/etc/sssd/sssd.conf` and add this parameter:

    ```
    selinux_provider=none
    ```

4.  Reboot the system to apply any changes that you made and verify that SELinux is deactivated.

If you choose to enable SELinux in `Enforcing` mode, then Greenplum processes and users can operate successfully in the default `Unconfined` context. If you require increased SELinux confinement for Greenplum processes and users, you must test your configuration to ensure that there are no functionality or performance impacts to Greenplum Database. See the [SELinux User's and Administrator's Guide](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/selinux_users_and_administrators_guide/index) for detailed information about configuring SELinux and SELinux users.

## <a id="topic_et2_y22_4nb"></a>Deactivate or Configure Firewall Software 

You should also deactivate firewall software such as `firewalld` \(on systems such as RHEL). If firewall software is not deactivated, you must instead configure your software to allow required communication between Greenplum hosts.

1. Check the status of `firewalld` with the command:

    ```
    # systemctl status firewalld
    ```

    If `firewalld` is deactivated, the command output is:

    ```
    * firewalld.service - firewalld - dynamic firewall daemon
       Loaded: loaded (/usr/lib/systemd/system/firewalld.service; disabled; vendor preset: enabled)
       Active: inactive (dead)
    ```

2.  If necessary, run these commands as root to deactivate `firewalld`:

    ```
    # systemctl stop firewalld.service
    # systemctl deactivate firewalld.service
    ```

See the documentation for the firewall or your operating system for additional information.

## <a id="topic3"></a>Recommended OS Parameters Settings 

Greenplum requires that certain Linux operating system \(OS\) parameters be set on all hosts in your Greenplum Database system \(coordinators and segments\).

In general, the following categories of system parameters need to be altered:

-   **Shared Memory** - A Greenplum Database instance will not work unless the shared memory segment for your kernel is properly sized. Most default OS installations have the shared memory values set too low for Greenplum Database. On Linux systems, you must also deactivate the OOM \(out of memory\) killer. For information about Greenplum Database shared memory requirements, see the Greenplum Database server configuration parameter [shared\_buffers](../ref_guide/config_params/guc-list.html) in the *Greenplum Database Reference Guide*.
-   **Network** - On high-volume Greenplum Database systems, certain network-related tuning parameters must be set to optimize network connections made by the Greenplum interconnect.
-   **User Limits** - User limits control the resources available to processes started by a user's shell. Greenplum Database requires a higher limit on the allowed number of file descriptors that a single process can have open. The default settings may cause some Greenplum Database queries to fail because they will run out of file descriptors needed to process the query.

More specifically, you need to edit the following Linux configuration settings:

-   [The hosts File](#linux_hosts_file)
-   [The sysctl.conf File](#sysctl_file)
-   [System Resources Limits](#system_resources)
-   [Core Dump](#core_dump)
-   [XFS Mount Options](#xfs_mount)
-   [Disk I/O Settings](#disk_io_settings)
    -   Read ahead values
    -   Disk I/O scheduler disk access
-   [Networking](#networking)
-   [Transparent Huge Pages \(THP\)](#huge_pages)
-   [IPC Object Removal](#ipc_object_removal)
-   [SSH Connection Threshold](#ssh_max_connections)

### <a id="linux_hosts_file"></a>The hosts File 

Edit the `/etc/hosts` file and make sure that it includes the host names and all interface address names for every machine participating in your Greenplum Database system.

### <a id="sysctl_file"></a>The sysctl.conf File 

The `sysctl.conf` parameters listed in this topic are for performance, optimization, and consistency in a wide variety of environments. Change these settings according to your specific situation and setup.

Set the parameters in the `/etc/sysctl.conf` file and reload with `sysctl -p`:

```

# kernel.shmall = _PHYS_PAGES / 2 # See Shared Memory Pages
kernel.shmall = 197951838
# kernel.shmmax = kernel.shmall * PAGE_SIZE 
kernel.shmmax = 810810728448
kernel.shmmni = 4096
vm.overcommit_memory = 2 # See Segment Host Memory
vm.overcommit_ratio = 95 # See Segment Host Memory

net.ipv4.ip_local_port_range = 10000 65535 # See Port Settings
kernel.sem = 250 2048000 200 8192
kernel.sysrq = 1
kernel.core_uses_pid = 1
kernel.msgmnb = 65536
kernel.msgmax = 65536
kernel.msgmni = 2048
net.ipv4.tcp_syncookies = 1
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.tcp_max_syn_backlog = 4096
net.ipv4.conf.all.arp_filter = 1
net.ipv4.ipfrag_high_thresh = 41943040
net.ipv4.ipfrag_low_thresh = 31457280
net.ipv4.ipfrag_time = 60
net.core.netdev_max_backlog = 10000
net.core.rmem_max = 2097152
net.core.wmem_max = 2097152
vm.swappiness = 10
vm.zone_reclaim_mode = 0
vm.dirty_expire_centisecs = 500
vm.dirty_writeback_centisecs = 100
vm.dirty_background_ratio = 0 # See System Memory
vm.dirty_ratio = 0
vm.dirty_background_bytes = 1610612736
vm.dirty_bytes = 4294967296
```

**Shared Memory Pages**

Greenplum Database uses shared memory to communicate between `postgres` processes that are part of the same `postgres` instance. `kernel.shmall` sets the total amount of shared memory, in pages, that can be used system wide. `kernel.shmmax` sets the maximum size of a single shared memory segment in bytes.

Set `kernel.shmall` and `kernel.shmmax` values based on your system's physical memory and page size. In general, the value for both parameters should be one half of the system physical memory.

Use the operating system variables `_PHYS_PAGES` and `PAGE_SIZE` to set the parameters.

```
kernel.shmall = ( _PHYS_PAGES / 2)
kernel.shmmax = ( _PHYS_PAGES / 2) * PAGE_SIZE
```

To calculate the values for `kernel.shmall` and `kernel.shmmax`, run the following commands using the `getconf` command, which returns the value of an operating system variable.

```
$ echo $(expr $(getconf _PHYS_PAGES) / 2) 
$ echo $(expr $(getconf _PHYS_PAGES) / 2 \* $(getconf PAGE_SIZE))
```

As best practice, we recommend you set the following values in the `/etc/sysctl.conf` file using calculated values. For example, a host system has 1583 GB of memory installed and returns these values: \_PHYS\_PAGES = 395903676 and PAGE\_SIZE = 4096. These would be the `kernel.shmall` and `kernel.shmmax` values:

```
kernel.shmall = 197951838
kernel.shmmax = 810810728448
```

If the Greenplum Database coordinator has a different shared memory configuration than the segment hosts, the \_PHYS\_PAGES and PAGE\_SIZE values might differ, and the `kernel.shmall` and `kernel.shmmax` values on the coordinator host will differ from those on the segment hosts.

**Segment Host Memory**

The `vm.overcommit_memory` Linux kernel parameter is used by the OS to determine how much memory can be allocated to processes. For Greenplum Database, this parameter should always be set to 2.

`vm.overcommit_ratio` is the percent of RAM that is used for application processes and the remainder is reserved for the operating system. The default is 50 on Red Hat Enterprise Linux.

For `vm.overcommit_ratio` tuning and calculation recommendations with resource group-based resource management or resource queue-based resource management, refer to [Options for Configuring Segment Host Memory](../admin_guide/wlmgmt_intro.html) in the *Geenplum Database Administrator Guide*.

**Port Settings**

To avoid port conflicts between Greenplum Database and other applications during Greenplum initialization, make a note of the port range specified by the operating system parameter `net.ipv4.ip_local_port_range`. When initializing Greenplum using the `gpinitsystem` cluster configuration file, do not specify Greenplum Database ports in that range. For example, if `net.ipv4.ip_local_port_range = 10000 65535`, set the Greenplum Database base port numbers to these values.

```
PORT_BASE = 6000
MIRROR_PORT_BASE = 7000
```

For information about the `gpinitsystem` cluster configuration file, see [Initializing a Greenplum Database System](../install_guide/init_gpdb.html).

For Azure deployments with Greenplum Database avoid using port 65330; add the following line to sysctl.conf:

```
net.ipv4.ip_local_reserved_ports=65330 
```

For additional requirements and recommendations for cloud deployments, see [Public Cloud Requirements](./platform-requirements-overview.html#public-cloud).

**IP Fragmentation Settings**

When the Greenplum Database interconnect uses UDP (the default), the network interface card controls IP packet fragmentation and reassemblies.

If the UDP message size is larger than the size of the maximum transmission unit (MTU) of a network, the IP layer fragments the message. (Refer to [Networking](#networking) later in this topic for more information about MTU sizes for Greenplum Database.) The receiver must store the fragments in a buffer before it can reorganize and reassemble the message.

The following `sysctl.conf` operating system parameters control the reassembly process:

| OS Parameter | Description |
|--------------|-------------|
| net.ipv4.ipfrag_high_thresh | The maximum amount of memory used to reassemble IP fragments before the kernel starts to remove fragments to free up resources. The default value is 4194304 bytes (4MB). |
| net.ipv4.ipfrag_low_thresh | The minimum amount of memory used to reassemble IP fragments. The default value is 3145728 bytes (3MB). (Deprecated after kernel version 4.17.) |
| net.ipv4.ipfrag_time | The maximum amount of time (in seconds) to keep an IP fragment in memory. The default value is 30. |

The recommended settings for these parameters for Greenplum Database follow:

``` pre
net.ipv4.ipfrag_high_thresh = 41943040
net.ipv4.ipfrag_low_thresh = 31457280
net.ipv4.ipfrag_time = 60
```

**System Memory**

For host systems with more than 64GB of memory, these settings are recommended:

```
vm.dirty_background_ratio = 0
vm.dirty_ratio = 0
vm.dirty_background_bytes = 1610612736 # 1.5GB
vm.dirty_bytes = 4294967296 # 4GB
```

For host systems with 64GB of memory or less, remove `vm.dirty_background_bytes` and `vm.dirty_bytes` and set the two `ratio` parameters to these values:

```
vm.dirty_background_ratio = 3
vm.dirty_ratio = 10
```

Increase `vm.min_free_kbytes` to ensure `PF_MEMALLOC` requests from network and storage drivers are easily satisfied. This is especially critical on systems with large amounts of system memory. The default value is often far too low on these systems. Use this awk command to set `vm.min_free_kbytes` to a recommended 3% of system physical memory:

```
awk 'BEGIN {OFMT = "%.0f";} /MemTotal/ {print "vm.min_free_kbytes =", $2 * .03;}'
               /proc/meminfo >> /etc/sysctl.conf 
```

Do not set `vm.min_free_kbytes` to higher than 5% of system memory as doing so might cause out of memory conditions.

### <a id="system_resources"></a>System Resources Limits 

Set the following parameters in the `/etc/security/limits.conf` file:

```
* soft nofile 524288
* hard nofile 524288
* soft nproc 131072
* hard nproc 131072
```

For Red Hat Enterprise Linux \(RHEL\) systems, parameter values in the `/etc/security/limits.d/20-nproc.conf` file override the values in the `limits.conf` file. Ensure that any parameters in the override file are set to the required value. The Linux module `pam_limits` sets user limits by reading the values from the `limits.conf` file and then from the override file. For information about PAM and user limits, see the documentation on PAM and `pam_limits`.

Run the `ulimit -u` command on each segment host to display the maximum number of processes that are available to each user. Validate that the return value is 131072.

### <a id="core_dump"></a>Core Dump 

Enable core file generation to a known location by adding the following line to `/etc/sysctl.conf`:

```
kernel.core_pattern=/var/core/core.%h.%t

```

Add the following line to `/etc/security/limits.conf`:

```
* soft  core unlimited
```

To apply the changes to the live kernel, run the following command:

```
# sysctl -p
```

### <a id="xfs_mount"></a>XFS Mount Options 

XFS is the preferred data storage file system on Linux platforms. Use the `mount` command with the following recommended XFS mount options for RHEL systems:

```
rw,nodev,noatime,inode64
```

See the `mount` manual page \(`man mount` opens the man page\) for more information about using this command.

The XFS options can also be set in the `/etc/fstab` file. This example entry from an `fstab` file specifies the XFS options.

```
/dev/data /data xfs nodev,noatime,inode64 0 0
```

> **Note** You must have root permission to edit the `/etc/fstab` file.

### <a id="disk_io_settings"></a>Disk I/O Settings 

-   Read-ahead value

    Each disk device file should have a read-ahead \(`blockdev`\) value of 16384. To verify the read-ahead value of a disk device:

    ```
    # sudo /sbin/blockdev --getra <devname>
    ```

    For example:

    ```
    # sudo /sbin/blockdev --getra /dev/sdb
    ```

    To set blockdev \(read-ahead\) on a device:

    ```
    # sudo /sbin/blockdev --setra <bytes> <devname>
    ```

    For example:

    ```
    # sudo /sbin/blockdev --setra 16384 /dev/sdb
    ```

    See the manual page \(man\) for the `blockdev` command for more information about using that command \(`man blockdev` opens the man page\).

    > **Note** The `blockdev --setra` command is not persistent. You must ensure the read-ahead value is set whenever the system restarts. How to set the value will vary based on your system.

    One method to set the `blockdev` value at system startup is by adding the `/sbin/blockdev --setra` command in the `rc.local` file. For example, add this line to the `rc.local` file to set the read-ahead value for the disk `sdb`.

    ```
    /sbin/blockdev --setra 16384 /dev/sdb
    ```

    On systems that use systemd, you must also set the execute permissions on the `rc.local` file to enable it to run at startup. For example, on a RHEL system, this command sets execute permissions on the file.

    ```
    # chmod +x /etc/rc.d/rc.local
    ```

    Restart the system to have the setting take effect.

-   Disk I/O scheduler

    The Linux disk scheduler orders the I/O requests submitted to a storage device, controlling the way the kernel commits reads and writes to disk.

    A typical Linux disk I/O scheduler supports multiple access policies. The optimal policy selection depends on the underlying storage infrastructure. The recommended scheduler policy settings for Greenplum Database systems for specific OSs and storage device types follow:

    <table>
      <thead>
        <tr>
          <th>Storage Device Type</th>
          <th>OS</th>
          <th>Recommended Scheduler Policy</th>
        </tr>
      </thead>
      <tbody>
        <tr>
          <td>Non-Volatile Memory Express (NVMe)</td>
          <td>RHEL 7</br>RHEL 8</br>Ubuntu</td>
          <td><code>none</code></td>
        </tr>
        <tr>
          <td rowspan="2">Solid-State Drives (SSD)</td>
          <td>RHEL 7</td>
          <td><code>noop</code></td>
        </tr>
        <tr>
          <td>RHEL 8</br>Ubuntu</td>
          <td><code>none</code></td>
        </tr>
        <tr>
          <td rowspan="2">Other</td>
          <td>RHEL 7</td>
          <td><code>deadline</code></td>
        </tr>
        <tr>
          <td>RHEL 8</br>Ubuntu</td>
          <td><code>mq-deadline</code></td>
        </tr>
      </tbody>
    </table>

    To specify a scheduler until the next system reboot, run the following:

    ```
    # echo schedulername > /sys/block/<devname>/queue/scheduler
    ```

    For example:

    ```
    # echo deadline > /sys/block/sbd/queue/scheduler
    ```

    > **Note** Using the `echo` command to set the disk I/O scheduler policy is not persistent; you must ensure that you run the command whenever the system reboots. How to run the command will vary based on your system.

    To specify the I/O scheduler at boot time on systems that use `grub2`, use the system utility `grubby`. This command adds the parameter when run as `root`:

    ```
    # grubby --update-kernel=ALL --args="elevator=deadline"
    ```

    After adding the parameter, reboot the system.

    This `grubby` command displays kernel parameter settings:

    ```
    # grubby --info=ALL
    ```

    Refer to your operating system documentation for more information about the `grubby` utility. If you used the `grubby` command to configure the disk scheduler on a RHEL system and it does not update the kernels, see the [Note](#grubby_note) at the end of the section.

    For additional information about configuring the disk scheduler, refer to the RedHat Enterprise Linux documentation for [RHEL 8](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/8/html/monitoring_and_managing_system_status_and_performance/setting-the-disk-scheduler_monitoring-and-managing-system-status-and-performance).


### <a id="networking"></a>Networking

The maximum transmission unit (MTU) of a network specifies the size (in bytes) of the largest data packet/frame accepted by a network-connected device. A jumbo frame is a frame that contains more than the standard MTU of 1500 bytes.

Greenplum Database utilizes 3 distinct MTU settings:

- The Greenplum Database [gp_max_packet_size](../ref_guide/config_params/guc-list.html#gp_max_packet_size) server configuration parameter. The default max packet size is 8192. This default assumes a jumbo frame MTU.
- The operating system MTU setting.
- The rack switch MTU setting.

These settings are connected, in that they should always be either the same, or close to the same, value, or otherwise in the order of Greenplum < OS < switch for MTU size.

9000 is a common supported setting for switches, and is the recommended OS and rack switch MTU setting for your Greenplum Database hosts.


### <a id="huge_pages"></a>Transparent Huge Pages \(THP\) 

Deactivate Transparent Huge Pages \(THP\) as it degrades Greenplum Database performance. RHEL 6.0 or higher enables THP by default. One way to deactivate THP on RHEL 6.x is by adding the parameter `transparent_hugepage=never` to the kernel command in the file `/boot/grub/grub.conf`, the GRUB boot loader configuration file. This is an example kernel command from a `grub.conf` file. The command is on multiple lines for readability:

```
kernel /vmlinuz-2.6.18-274.3.1.el5 ro root=LABEL=/
           elevator=deadline crashkernel=128M@16M  quiet console=tty1
           console=ttyS1,115200 panic=30 transparent_hugepage=never 
           initrd /initrd-2.6.18-274.3.1.el5.img
```

On systems that use `grub2`, use the system utility `grubby`. This command adds the parameter when run as root.

```
# grubby --update-kernel=ALL --args="transparent_hugepage=never"
```

After adding the parameter, reboot the system.

For Ubuntu systems, install the `hugepages` package and run this command as root:

```
# hugeadm --thp-never
```

This cat command checks the state of THP. The output indicates that THP is deactivated.

```
$ cat /sys/kernel/mm/*transparent_hugepage/enabled
always [never]
```

For more information about Transparent Huge Pages or the `grubby` utility, see your operating system documentation. If the `grubby` command does not update the kernels, see the [Note](#grubby_note) at the end of the section.

### <a id="ipc_object_removal"></a>IPC Object Removal 

Deactivate IPC object removal. The default `systemd` setting `RemoveIPC=yes` removes IPC connections when non-system user accounts log out. This causes the Greenplum Database utility `gpinitsystem` to fail with semaphore errors. Perform one of the following to avoid this issue.

-   When you add the `gpadmin` operating system user account to the coordinator node in [Creating the Greenplum Administrative User](#topic23), create the user as a system account.
-   Deactivate `RemoveIPC`. Set this parameter in `/etc/systemd/logind.conf` on the Greenplum Database host systems.

    ```
    RemoveIPC=no
    ```

    The setting takes effect after restarting the `systemd-login` service or rebooting the system. To restart the service, run this command as the root user.

    ```
    service systemd-logind restart
    ```


### <a id="ssh_max_connections"></a>SSH Connection Threshold 

Certain Greenplum Database management utilities including `gpexpand`, `gpinitsystem`, and `gpaddmirrors`, use secure shell \(SSH\) connections between systems to perform their tasks. In large Greenplum Database deployments, cloud deployments, or deployments with a large number of segments per host, these utilities may exceed the host's maximum threshold for unauthenticated connections. When this occurs, you receive errors such as: `ssh_exchange_identification: Connection closed by remote host`.

To increase this connection threshold for your Greenplum Database system, update the SSH `MaxStartups` and `MaxSessions` configuration parameters in one of the `/etc/ssh/sshd_config` or `/etc/sshd_config` SSH daemon configuration files.

> **Note** You must have root permission to edit these two files.

If you specify `MaxStartups` and `MaxSessions` using a single integer value, you identify the maximum number of concurrent unauthenticated connections \(`MaxStartups`\) and maximum number of open shell, login, or subsystem sessions permitted per network connection \(`MaxSessions`\). For example:

```
MaxStartups 200
MaxSessions 200
```

If you specify `MaxStartups` using the "start:rate:full" syntax, you enable random early connection drop by the SSH daemon. start identifies the maximum number of unauthenticated SSH connection attempts allowed. Once start number of unauthenticated connection attempts is reached, the SSH daemon refuses rate percent of subsequent connection attempts. full identifies the maximum number of unauthenticated connection attempts after which all attempts are refused. For example:

```
Max Startups 10:30:200
MaxSessions 200
```

Restart the SSH daemon after you update `MaxStartups` and `MaxSessions`. For example, on a CentOS 6 system, run the following command as the `root` user:

```
# service sshd restart
```

For detailed information about SSH configuration options, refer to the SSH documentation for your Linux distribution.

<a id="grubby_note"></a>

> **Note** If the `grubby` command does not update the kernels of a RHEL 7.x or CentOS 7.x system, you can manually update all kernels on the system. For example, to add the parameter `transparent_hugepage=never` to all kernels on a system.

1.  Add the parameter to the `GRUB_CMDLINE_LINUX` line in the file parameter in `/etc/default/grub`.

    ```
    GRUB_TIMEOUT=5
    GRUB_DISTRIBUTOR="$(sed 's, release .*$,,g' /etc/system-release)"
    GRUB_DEFAULT=saved
    GRUB_DISABLE_SUBMENU=true
    GRUB_TERMINAL_OUTPUT="console"
    GRUB_CMDLINE_LINUX="crashkernel=auto rd.lvm.lv=cl/root rd.lvm.lv=cl/swap rhgb quiet transparent_hugepage=never"
                  GRUB_DISABLE_RECOVERY="true"
    ```

    > **Note** You must have root permission to edit the `/etc/default/grub` file.

2.  As root, run the `grub2-mkconfig` command to update the kernels.

    ```
    # grub2-mkconfig -o /boot/grub2/grub.cfg
    ```

3.  Reboot the system.

## <a id="topic_qst_s5t_wy"></a>Synchronizing System Clocks 

You should use NTP \(Network Time Protocol\) to synchronize the system clocks on all hosts that comprise your Greenplum Database system. See [www.ntp.org](http://www.ntp.org) for more information about NTP.

NTP on the segment hosts should be configured to use the coordinator host as the primary time source, and the standby coordinator as the secondary time source. On the coordinator and standby coordinator hosts, configure NTP to point to your preferred time server.

### <a id="ji162603"></a>To configure NTP 

1.  On the coordinator host, log in as root and edit the `/etc/ntp.conf` file. Set the `server` parameter to point to your data center's NTP time server. For example \(if `10.6.220.20` was the IP address of your data center's NTP server\):

    ```
    server 10.6.220.20
    ```

2.  On each segment host, log in as root and edit the `/etc/ntp.conf` file. Set the first `server` parameter to point to the coordinator host, and the second server parameter to point to the standby coordinator host. For example:

    ```
    server cdw prefer
    server scdw
    ```

3.  On the standby coordinator host, log in as root and edit the `/etc/ntp.conf` file. Set the first `server` parameter to point to the primary coordinator host, and the second server parameter to point to your data center's NTP time server. For example:

    ```
    server cdw prefer
    server 10.6.220.20
    ```

4.  On the coordinator host, use the NTP daemon synchronize the system clocks on all Greenplum hosts. For example, using [gpssh](../utility_guide/ref/gpssh.html):

    ```
    # gpssh -f hostfile_gpssh_allhosts -v -e 'ntpd'
    ```


## <a id="topic23"></a>Creating the Greenplum Administrative User 

Create a dedicated operating system user account on each node to run and administer Greenplum Database. This user account is named `gpadmin` by convention.

> **Important** You cannot run the Greenplum Database server as `root`.

The `gpadmin` user must have permission to access the services and directories required to install and run Greenplum Database.

The `gpadmin` user on each Greenplum host must have an SSH key pair installed and be able to SSH from any host in the cluster to any other host in the cluster without entering a password or passphrase \(called "passwordless SSH"\). If you enable passwordless SSH from the coordinator host to every other host in the cluster \("1-*n* passwordless SSH"\), you can use the Greenplum Database `gpssh-exkeys` command-line utility later to enable passwordless SSH from every host to every other host \("*n*-*n* passwordless SSH"\).

You can optionally give the `gpadmin` user sudo privilege, so that you can easily administer all hosts in the Greenplum Database cluster as `gpadmin` using the `sudo`, `ssh/rsync`, and `gpssh/gpsync` commands.

The following steps show how to set up the `gpadmin` user on a host, set a password, create an SSH key pair, and \(optionally\) enable sudo capability. These steps must be performed as root on every Greenplum Database cluster host. \(For a large Greenplum Database cluster you will want to automate these steps using your system provisioning tools.\)

> **Note** See [Example Ansible Playbook](ansible-example.html) for an example that shows how to automate the tasks of creating the `gpadmin` user and installing the Greenplum Database software on all hosts in the cluster.

1.  Create the `gpadmin` group and user.

    > **Note** If you are installing Greenplum Database on RHEL 7.2 or CentOS 7.2 and want to deactivate IPC object removal by creating the `gpadmin` user as a system account, provide both the `-r` option \(create the user as a system account\) and the `-m` option \(create a home directory\) to the `useradd` command. On Ubuntu systems, you must use the `-m` option with the `useradd` command to create a home directory for a user.

    This example creates the `gpadmin` group, creates the `gpadmin` user as a system account with a home directory and as a member of the `gpadmin` group, and creates a password for the user.

    ```
    # groupadd gpadmin
    # useradd gpadmin -r -m -g gpadmin
    # passwd gpadmin
    New password: <changeme>
    Retype new password: <changeme>
    ```

    > **Note** You must have root permission to create the `gpadmin` group and user.

    > **Note** Make sure the `gpadmin` user has the same user id \(uid\) and group id \(gid\) numbers on each host to prevent problems with scripts or services that use them for identity or permissions. For example, backing up Greenplum databases to some networked filesy stems or storage appliances could fail if the `gpadmin` user has different uid or gid numbers on different segment hosts. When you create the `gpadmin` group and user, you can use the `groupadd -g` option to specify a gid number and the `useradd -u` option to specify the uid number. Use the command `id gpadmin` to see the uid and gid for the `gpadmin` user on the current host.

2.  Switch to the `gpadmin` user and generate an SSH key pair for the `gpadmin` user.

    ```
    $ su gpadmin
    $ ssh-keygen -t rsa -b 4096
    Generating public/private rsa key pair.
    Enter file in which to save the key (/home/gpadmin/.ssh/id_rsa):
    Created directory '/home/gpadmin/.ssh'.
    Enter passphrase (empty for no passphrase):
    Enter same passphrase again:
    
    ```

    At the passphrase prompts, press Enter so that SSH connections will not require entry of a passphrase.

3.  Grant sudo access to the `gpadmin` user.

    On Red Hat or CentOS, run `visudo` and uncomment the `%wheel` group entry.

    ```
    %wheel        ALL=(ALL)       NOPASSWD: ALL
    ```

    Make sure you uncomment the line that has the `NOPASSWD` keyword.

    Add the `gpadmin` user to the `wheel` group with this command.

    ```
    # usermod -aG wheel gpadmin
    ```


## <a id="topic_acx_5xb_vhb"></a>Next Steps 

-   [Installing Greenplum Database Software](install_gpdb.html)
-   [Validating Your Systems](validate.html)
-   [Initializing a Greenplum Database System](init_gpdb.html)

