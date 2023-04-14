---
title: Enabling iptables (Optional)
---

On Linux systems, you can configure and enable the `iptables` firewall to work with Greenplum Database.

> **Note** Greenplum Database performance might be impacted when `iptables` is enabled. You should test the performance of your application with `iptables` enabled to ensure that performance is acceptable.

For more information about `iptables` see the `iptables` and firewall documentation for your operating system. See also [Deactivating SELinux and Firewall Software](prep_os.html).

## <a id="ji163124"></a>How to Enable iptables 

1.  As `gpadmin`, run this command on the Greenplum Database coordinator host to stop Greenplum Database:

    ```
    $ gpstop -a
    ```

2.  On the Greenplum Database hosts:
    1.  Update the file `/etc/sysconfig/iptables` based on the [Example iptables Rules](#topic16).
    2.  As root user, run these commands to enable `iptables`:

        ```
        # chkconfig iptables on
        # service iptables start
        ```

3.  As gpadmin, run this command on the Greenplum Database coordinator host to start Greenplum Database:

    ```
    $ gpstart -a
    ```


> **Caution** After enabling `iptables`, this error in the `/var/log/messages` file indicates that the setting for the `iptables` table is too low and needs to be increased.

```
ip_conntrack: table full, dropping packet.
```

As root, run this command to view the `iptables` table value:

```
# sysctl net.ipv4.netfilter.ip_conntrack_max
```

To ensure that the Greenplum Database workload does not overflow the `iptables` table, as root, set it to the following value:

```
# sysctl net.ipv4.netfilter.ip_conntrack_max=6553600
```

The value might need to be adjusted for your hosts. To maintain the value after reboot, you can update the `/etc/sysctl.conf` file as discussed in [Setting the Greenplum Recommended OS Parameters](prep_os.html).

**Parent topic:** [Installing and Upgrading Greenplum](install_guide.html)

## <a id="topic16"></a>Example iptables Rules 

When `iptables` is enabled, `iptables` manages the IP communication on the host system based on configuration settings \(rules\). The example rules are used to configure `iptables` for Greenplum Database coordinator host, standby coordinator host, and segment hosts.

-   [Example Coordinator and Standby Coordinator iptables Rules](#topic17)
-   [Example Segment Host iptables Rules](#topic18)

The two sets of rules account for the different types of communication Greenplum Database expects on the coordinator \(primary and standby\) and segment hosts. The rules should be added to the `/etc/sysconfig/iptables` file of the Greenplum Database hosts. For Greenplum Database, `iptables` rules should allow the following communication:

-   For customer facing communication with the Greenplum Database coordinator, allow at least `postgres` and `28080` \(`eth1` interface in the example\).
-   For Greenplum Database system interconnect, allow communication using `tcp`, `udp`, and `icmp` protocols \(`eth4` and `eth5` interfaces in the example\).

    The network interfaces that you specify in the `iptables` settings are the interfaces for the Greenplum Database hosts that you list in the hostfile\_gpinitsystem file. You specify the file when you run the `gpinitsystem` command to initialize a Greenplum Database system. See [Initializing a Greenplum Database System](init_gpdb.html) for information about the hostfile\_gpinitsystem file and the `gpinitsystem` command.

-   For the administration network on a Greenplum DCA, allow communication using `ssh`, `ntp`, and `icmp` protocols. \(`eth0` interface in the example\).

In the `iptables` file, each append rule command \(lines starting with `-A`\) is a single line.

The example rules should be adjusted for your configuration. For example:

-   The append command, the `-A` lines and connection parameter `-i` should match the connectors for your hosts.
-   the CIDR network mask information for the source parameter `-s` should match the IP addresses for your network.

### <a id="topic17"></a>Example Coordinator and Standby Coordinator iptables Rules 

Example `iptables` rules with comments for the `/etc/sysconfig/iptables` file on the Greenplum Database coordinator host and standby coordinator host.

```
*filter
# Following 3 are default rules. If the packet passes through
# the rule set it gets these rule.
# Drop all inbound packets by default.
# Drop all forwarded (routed) packets.
# Let anything outbound go through.
:INPUT DROP [0:0]
:FORWARD DROP [0:0]
:OUTPUT ACCEPT [0:0]
# Accept anything on the loopback interface.
-A INPUT -i lo -j ACCEPT
# If a connection has already been established allow the
# remote host packets for the connection to pass through.
-A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT
# These rules let all tcp and udp through on the standard
# interconnect IP addresses and on the interconnect interfaces.
# NOTE: gpsyncmaster uses random tcp ports in the range 1025 to 65535
# and Greenplum Database uses random udp ports in the range 1025 to 65535.
-A INPUT -i eth4 -p udp -s 192.0.2.0/22 -j ACCEPT
-A INPUT -i eth5 -p udp -s 198.51.100.0/22 -j ACCEPT
-A INPUT -i eth4 -p tcp -s 192.0.2.0/22 -j ACCEPT --syn -m state --state NEW
-A INPUT -i eth5 -p tcp -s 198.51.100.0/22 -j ACCEPT --syn -m state --state NEW
\# Allow udp/tcp ntp connections on the admin network on Greenplum DCA.
-A INPUT -i eth0 -p udp --dport ntp -s 203.0.113.0/21 -j ACCEPT
-A INPUT -i eth0 -p tcp --dport ntp -s 203.0.113.0/21 -j ACCEPT --syn -m state --state NEW
# Allow ssh on all networks (This rule can be more strict).
-A INPUT -p tcp --dport ssh -j ACCEPT --syn -m state --state NEW
# Allow Greenplum Database on all networks.
-A INPUT -p tcp --dport postgres -j ACCEPT --syn -m state --state NEW
# Allow Greenplum Command Center on the customer facing network.
-A INPUT -i eth1 -p tcp --dport 28080 -j ACCEPT --syn -m state --state NEW
# Allow ping and any other icmp traffic on the interconnect networks.
-A INPUT -i eth4 -p icmp -s 192.0.2.0/22 -j ACCEPT
-A INPUT -i eth5 -p icmp -s 198.51.100.0/22 -j ACCEPT
\# Allow ping only on the admin network on Greenplum DCA.
-A INPUT -i eth0 -p icmp --icmp-type echo-request -s 203.0.113.0/21 -j ACCEPT
# Log an error if a packet passes through the rules to the default
# INPUT rule (a DROP).
-A INPUT -m limit --limit 5/min -j LOG --log-prefix "iptables denied: " --log-level 7
COMMIT
```

### <a id="topic18"></a>Example Segment Host iptables Rules 

Example `iptables` rules for the `/etc/sysconfig/iptables` file on the Greenplum Database segment hosts. The rules for segment hosts are similar to the coordinator rules with fewer interfaces and fewer `udp` and `tcp` services.

```
*filter
:INPUT DROP
:FORWARD DROP
:OUTPUT ACCEPT
-A INPUT -i lo -j ACCEPT
-A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT
-A INPUT -i eth2 -p udp -s 192.0.2.0/22 -j ACCEPT
-A INPUT -i eth3 -p udp -s 198.51.100.0/22 -j ACCEPT
-A INPUT -i eth2 -p tcp -s 192.0.2.0/22 -j ACCEPT --syn -m state --state NEW
-A INPUT -i eth3 -p tcp -s 198.51.100.0/22 -j ACCEPT --syn -m state --state NEW
-A INPUT -p tcp --dport ssh -j ACCEPT --syn -m state --state NEW
-A INPUT -i eth2 -p icmp -s 192.0.2.0/22 -j ACCEPT
-A INPUT -i eth3 -p icmp -s 198.51.100.0/22 -j ACCEPT
-A INPUT -i eth0 -p icmp --icmp-type echo-request -s 203.0.113.0/21 -j ACCEPT
-A INPUT -m limit --limit 5/min -j LOG --log-prefix "iptables denied: " --log-level 7
COMMIT
```

