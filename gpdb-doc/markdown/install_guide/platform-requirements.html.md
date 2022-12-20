---
title: Platform Requirements for On-Premise Hardware
---

This topic describes the Greenplum Database 6 platform and operating system software requirements.

-   [Operating Systems](#topic13)
    -   [Software Dependencies](#topic_i4k_nlx_zgb)
    -   [Java](#topic_xbl_mkx_zgb)
-   [Hardware and Network](#topic_tnl_3mx_zgb)
-   [Storage](#topic_pnz_5zd_xs)
-   [Tanzu Greenplum Tools and Extensions Compatibility](#topic31)
    -   [Client Tools](#topic32)
    -   [Extensions](#topic_eyc_l2h_zz)
    -   [Data Connectors](#topic_xpf_25b_hbb)
    -   [Tanzu Greenplum Text](#topic_ncl_w1d_r1b)
    -   [Greenplum Command Center](#topic_zkq_j5b_hbb)
-   [Hadoop Distributions](#topic36)

**Parent topic:** [Installing and Upgrading Greenplum](install_guide.html)

## <a id="topic13"></a>Operating Systems 

Greenplum Database 6 runs on the following operating system platforms:

-   Red Hat Enterprise Linux 64-bit 8.x (as of Greenplum Database version 6.20)
-   Red Hat Enterprise Linux 64-bit 7.x \(See the following [Note](#7x-issue).\)
-   Red Hat Enterprise Linux 64-bit 6.x
-   CentOS 64-bit 7.x
-   CentOS 64-bit 6.x
-   Ubuntu 18.04 LTS
-   Oracle Linux 64-bit 7, using the Red Hat Compatible Kernel \(RHCK\)

**Important:** Significant Greenplum Database performance degradation has been observed when enabling resource group-based workload management on RedHat 6.x and CentOS 6.x systems. This issue is caused by a Linux cgroup kernel bug. This kernel bug has been fixed in CentOS 7.x and Red Hat 7.x/8.x systems.

If you use RedHat 6 and the performance with resource groups is acceptable for your use case, upgrade your kernel to version 2.6.32-696 or higher to benefit from other fixes to the cgroups implementation.

**Note:** For Greenplum Database that is installed on Red Hat Enterprise Linux 7.x or CentOS 7.x prior to 7.3, an operating system issue might cause Greenplum Database that is running large workloads to hang in the workload. The Greenplum Database issue is caused by Linux kernel bugs.

RHEL 7.3 and CentOS 7.3 resolves the issue.

**Note:** Do not install anti-virus software on Greenplum Database hosts as the software might cause extra CPU and IO load that interferes with Greenplum Database operations.

Greenplum Database server supports TLS version 1.2 on RHEL/CentOS systems, and TLS version 1.3 on Ubuntu systems.

### <a id="topic_i4k_nlx_zgb"></a>Software Dependencies 

Greenplum Database 6 requires the following software packages on RHEL/CentOS 6/7 systems which are installed automatically as dependencies when you install the Greenplum RPM package\):

-   apr
-   apr-util
-   bash
-   bzip2
-   curl
-   krb5
-   libcurl
-   libevent
-   libxml2
-   libyaml
-   zlib
-   openldap
-   openssh-client
-   openssl
-   openssl-libs \(RHEL7/Centos7\)
-   perl
-   readline
-   rsync
-   R
-   sed \(used by `gpinitsystem`\)
-   tar
-   zip

Tanzu Greenplum Database 6 client software requires these operating system packages:

-   apr
-   apr-util
-   libyaml
-   libevent

On Ubuntu systems, Greenplum Database 6 requires the following software packages, which are installed automatically as dependencies when you install Greenplum Database with the Debian package installer:

-   libapr1
-   libaprutil1
-   bash
-   bzip2
-   krb5-multidev
-   libcurl3-gnutls
-   libcurl4
-   libevent-2.1-6
-   libxml2
-   libyaml-0-2
-   zlib1g
-   libldap-2.4-2
-   openssh-client
-   openssl
-   perl
-   readline
-   rsync
-   sed
-   tar
-   zip
-   net-tools
-   less
-   iproute2

Greenplum Database 6 uses Python 2.7.18, which is included with the product installation \(and not installed as a package dependency\).

**Important:** SSL is supported only on the Greenplum Database master host system. It cannot be used on the segment host systems.

**Important:** For all Greenplum Database host systems, if SELinux is enabled in `Enforcing` mode then the Greenplum process and users can operate successfully in the default `Unconfined` context. If increased confinement is required, then you must configure SELinux contexts, policies, and domains based on your security requirements, and test your configuration to ensure there is no functionality or performance impact to Greenplum Database. Similarly, you should either deactivate or configure firewall software as needed to allow communication between Greenplum hosts. See [Deactivate or Configure SELinux](prep_os.html).

### <a id="topic_xbl_mkx_zgb"></a>Java 

Greenplum Databased 6 supports these Java versions for PL/Java and PXF:

-   Open JDK 8 or Open JDK 11, available from [AdoptOpenJDK](https://adoptopenjdk.net)
-   Oracle JDK 8 or Oracle JDK 11

## <a id="topic_tnl_3mx_zgb"></a>Hardware and Network 

The following table lists minimum recommended specifications for hardware servers intended to support Greenplum Database on Linux systems in a production environment. All host servers in your Greenplum Database system must have the same hardware and software configuration. Greenplum also provides hardware build guides for its certified hardware platforms. It is recommended that you work with a Greenplum Systems Engineer to review your anticipated environment to ensure an appropriate hardware configuration for Greenplum Database.

<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" id="topic_tnl_3mx_zgb__ji162790" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap">Minimum Hardware Requirements</span></caption><colgroup><col style="width:120pt" /><col style="width:255pt" /></colgroup><tbody class="tbody">
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;">Minimum CPU</td>
<td class="entry cell-norowborder" style="vertical-align:top;">Any x86_64 compatible CPU</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;">Minimum Memory</td>
<td class="entry cell-norowborder" style="vertical-align:top;">16 GB RAM per server</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;">Disk Space Requirements</td>
<td class="entry cell-norowborder" style="vertical-align:top;">
<ul class="ul" id="topic_tnl_3mx_zgb__ul_us1_b4n_r4">
<li class="li">150MB per host for Greenplum installation</li>
<li class="li">Approximately 300MB per segment instance for metadata</li>
<li class="li">Cap disk capacity at 70% full to accommodate temporary files and prevent performance degradation</li>
</ul>
</td>
</tr>
<tr class="row">
<td class="entry row-nocellborder" style="vertical-align:top;">Network Requirements</td>
<td class="entry cellrowborder" style="vertical-align:top;">10 Gigabit Ethernet within the array<p class="p">NIC bonding is
                  recommended when multiple interfaces are present</p>
<p class="p">Greenplum Database can use
                  either IPV4 or IPV6 protocols.</p>
</td>
</tr>
</tbody>
</table>
</div>

### <a id="topic_elb_4ss_n4b"></a>Tanzu Greenplum on DCA Systems 

You must run Tanzu Greenplum version 6.9 or later on Dell EMC DCA systems, with software version 4.2.0.0 and later.

## <a id="topic_pnz_5zd_xs"></a>Storage 

The only file system supported for running Greenplum Database is the XFS file system. All other file systems are explicitly *not* supported by VMware.

Greenplum Database is supported on network or shared storage if the shared storage is presented as a block device to the servers running Greenplum Database and the XFS file system is mounted on the block device. Network file systems are *not* supported. When using network or shared storage, Greenplum Database mirroring must be used in the same way as with local storage, and no modifications may be made to the mirroring scheme or the recovery scheme of the segments.

Other features of the shared storage such as de-duplication and/or replication are not directly supported by Greenplum Database, but may be used with support of the storage vendor as long as they do not interfere with the expected operation of Greenplum Database at the discretion of VMware.

Greenplum Database can be deployed to virtualized systems only if the storage is presented as block devices and the XFS file system is mounted for the storage of the segment directories.

Greenplum Database is supported on Amazon Web Services \(AWS\) servers using either Amazon instance store \(Amazon uses the volume names `ephemeral[0-23]`\) or Amazon Elastic Block Store \(Amazon EBS\) storage. If using Amazon EBS storage the storage should be RAID of Amazon EBS volumes and mounted with the XFS file system for it to be a supported configuration.

### <a id="fixme"></a>Data Domain Boost \(Tanzu Greenplum\) 

Tanzu Greenplum 6 supports Data Domain Boost for backup on Red Hat Enterprise Linux. This table lists the versions of Data Domain Boost SDK and DDOS supported by Tanzu Greenplum 6.

|Tanzu Greenplum|Data Domain Boost|DDOS|
|---------------|-----------------|----|
|6.x|3.3|6.1 \(all versions\), 6.0 \(all versions\)|

**Note:** In addition to the DDOS versions listed in the previous table, Tanzu Greenplum supports all minor patch releases \(fourth digit releases\) later than the certified version.

## <a id="topic31"></a>Tanzu Greenplum Tools and Extensions Compatibility 

-   [Client Tools](#topic32) \(Tanzu Greenplum\)
-   [Extensions](#topic_eyc_l2h_zz)
-   [Data Connectors](#topic_xpf_25b_hbb)
-   [Tanzu Greenplum Text](#topic_ncl_w1d_r1b)
-   [Greenplum Command Center](#topic_zkq_j5b_hbb)

### <a id="topic32"></a>Client Tools 

VMware releases a Clients tool package on various platforms that can be used to access Greenplum Database from a client system. The Greenplum 6 Clients tool package is supported on the following platforms:

-   Red Hat Enterprise Linux x86\_64 6.x \(RHEL 6\)
-   Red Hat Enterprise Linux x86\_64 7.x \(RHEL 7\)
-   Red Hat Enterprise Linux x86\_64 8.x \(RHEL 8\)
-   Ubuntu 18.04 LTS
-   SUSE Linux Enterprise Server x86\_64 12 \(SLES 12\)
-   Windows 10 \(32-bit and 64-bit\)
-   Windows 8 \(32-bit and 64-bit\)
-   Windows Server 2012 \(32-bit and 64-bit\)
-   Windows Server 2012 R2 \(32-bit and 64-bit\)
-   Windows Server 2008 R2 \(32-bit and 64-bit\)

The Greenplum 6 Clients package includes the client and loader programs provided in the Greenplum 5 packages plus the addition of database/role/language commands and the Greenplum Streaming Server command utilities. Refer to [Greenplum Client and Loader Tools Package](/vmware/client_tool_guides/intro.html) for installation and usage details of the Greenplum 6 Client tools.

### <a id="topic_eyc_l2h_zz"></a>Extensions 

This table lists the versions of the Greenplum Extensions that are compatible with this release of Greenplum Database 6.

<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" id="topic_eyc_l2h_zz__table_b1q_m2h_zz" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap">Greenplum Extensions Compatibility </span></caption><colgroup><col /><col /><col /></colgroup><thead class="thead" style="text-align:left;">
<tr class="row">
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d78288e683">Component</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d78288e686">Package Version</th>
<th class="entry cell-norowborder" style="vertical-align:top;" id="d78288e689">Additional Information</th>
</tr>
</thead>
<tbody class="tbody">
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../analytics/pl_java.html">PL/Java</a></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">2.0.4</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 ">Supports Java 8 and 11.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../install_guide/install_python_dsmod.html">Python Data Science Module Package</a></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">2.0.2</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../analytics/pl_r.html">PL/R</a></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">3.0.3</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 ">(CentOS) R 3.3.3<p class="p"> (Ubuntu) You install R 3.5.1+.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../install_guide/install_r_dslib.html">R Data Science Library Package</a></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">2.0.2</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../analytics/pl_container.html">PL/Container</a></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">2.1.2</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 ">PL/Container Image for R </td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">2.1.2</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 ">R 3.6.3</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 ">PL/Container Images for Python </td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">2.1.2</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 ">Python 2.7.18<p class="p">Python 3.7</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 ">PL/Container Beta</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">3.0.0-beta</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 ">PL/Container Beta Image for R </td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">3.0.0-beta</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 ">R 3.4.4</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../analytics/greenplum_r_client.html">GreenplumR</a></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">1.1.0</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 ">Supports R 3.6+.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../analytics/madlib.html">MADlib Machine Learning</a></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d78288e686 ">1.19, 1.18, 1.17, 1.16</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d78288e689 ">Support matrix at <a class="xref" href="https://cwiki.apache.org/confluence/display/MADLIB/FAQ#FAQ-Q1-2WhatdatabaseplatformsdoesMADlibsupportandwhatistheupgradematrix?" target="_blank">MADlib FAQ</a>.</td>
</tr>
<tr class="row">
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d78288e683 "><a class="xref" href="../analytics/postGIS.html">PostGIS Spatial and Geographic Objects</a></td>
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d78288e686 ">2.5.4+pivotal.7.build.1, 2.1.5+pivotal.3.build.3</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d78288e689 "> </td>
</tr>
</tbody>
</table>
</div>

For information about the Oracle Compatibility Functions, see [Oracle Compatibility Functions](../ref_guide/modules/orafce_ref.html).

These Greenplum Database extensions are installed with Greenplum Database

-   Fuzzy String Match Extension
-   PL/Python Extension
-   pgcrypto Extension

### <a id="topic_xpf_25b_hbb"></a>Data Connectors 

-   Greenplum Platform Extension Framework \(PXF\) - PXF provides access to Hadoop, object store, and SQL external data stores. Refer to [Accessing External Data with PXF](../admin_guide/external/pxf-overview.html) in the *Greenplum Database Administrator Guide* for PXF configuration and usage information.

    **Note:** VMware Tanzu Greenplum Database versions starting with 6.19.0 no longer bundle a version of PXF. You can install PXF in your Greenplum cluster by installing [the independent distribution of PXF](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Platform-Extension-Framework/index.html) as described in the PXF documentation.
-   Greenplum Streaming Server v1.5.3 - The Tanzu Greenplum Streaming Server is an ETL tool that provides high speed, parallel data transfer from Informatica, Kafka, Apache NiFi and custom client data sources to a Tanzu Greenplum cluster. Refer to the [Tanzu Greenplum Streaming Server](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Streaming-Server/index.html) Documentation for more information about this feature.
-   Greenplum Streaming Server Kafka integration - The Kafka integration provides high speed, parallel data transfer from a Kafka cluster to a Greenplum Database cluster for batch and streaming ETL operations. It requires Kafka version 0.11 or newer for exactly-once delivery assurance. Refer to the [Tanzu Greenplum Streaming Server](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Streaming-Server/index.html) Documentation for more information about this feature.
-   Greenplum Connector for Apache Spark v1.6.2 - The Tanzu Greenplum Connector for Apache Spark supports high speed, parallel data transfer between Greenplum and an Apache Spark cluster using Spark’s Scala API.
-   Greenplum Connector for Apache NiFi v1.0.0 - The Tanzu Greenplum Connector for Apache NiFi enables you to set up a NiFi dataflow to load record-oriented data from any source into Greenplum Database.
-   Greenplum Informatica Connector v1.0.5 - The Tanzu Greenplum Connector for Informatica supports high speed data transfer from an Informatica PowerCenter cluster to a Tanzu Greenplum cluster for batch and streaming ETL operations.
-   Progress DataDirect JDBC Drivers v5.1.4+275, v6.0.0+181 - The Progress DataDirect JDBC drivers are compliant with the Type 4 architecture, but provide advanced features that define them as Type 5 drivers.
-   Progress DataDirect ODBC Drivers v7.1.6+7.16.389 - The Progress DataDirect ODBC drivers enable third party applications to connect via a common interface to the Tanzu Greenplum system.
-   R2B X-LOG v5.x and v6.x - Real-time data replication solution that achieves high-speed database replication through the use of Redo Log Capturing method.

**Note:** Greenplum 5.x clients (gpload, gpfdist) are supported with Greenplum 6.x Server and Informatica PowerCenter and PowerExchange 10.4.

**Note:** Tanzu Greenplum 6 does not support the ODBC driver for Cognos Analytics V11.

Connecting to IBM Cognos software with an ODBC driver is not supported. Greenplum Database supports connecting to IBM Cognos software with the DataDirect JDBC driver for Tanzu Greenplum. This driver is available as a download from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).

### <a id="topic_ncl_w1d_r1b"></a>Tanzu Greenplum Text 

Tanzu Greenplum 6.0 through 6.4 are compatible with Tanzu Greenplum Text 3.3.1 through 3.4.1. Tanzu Greenplum 6.5 and later are compatible with Tanzu Greenplum Text 3.4.2 and later. See the [Greenplum Text documentation](http://docs.vmware.com/en/VMware-Tanzu-Greenplum-Text/index.html) for additional compatibility information.

### <a id="topic_zkq_j5b_hbb"></a>Greenplum Command Center 

Tanzu Greenplum 6.15 is compatible only with Tanzu Greenplum Command Center 6.4.0 and later. See the [Greenplum Command Center documentation](http://docs.vmware.com/en/VMware-Tanzu-Greenplum-Command-Center/index.html) for additional compatibility information.

## <a id="topic36"></a>Hadoop Distributions 

Greenplum Database provides access to HDFS with the [Greenplum Platform Extension Framework \(PXF\)](https://docs.vmware.com/en/VMware-Tanzu-Greenplum-Platform-Extension-Framework/index.html).

PXF can use Cloudera, Hortonworks Data Platform, MapR, and generic Apache Hadoop distributions. PXF bundles all of the JAR files on which it depends, including the following Hadoop libraries:

|PXF Version|Hadoop Version|Hive Server Version|HBase Server Version|
|-----------|--------------|-------------------|--------------------|
|6.x, 5.15.x, 5.14.0, 5.13.0, 5.12.0, 5.11.1, 5.10.1|2.x, 3.1+|1.x, 2.x, 3.1+|1.3.2|
|5.8.2|2.x|1.x|1.3.2|
|5.8.1|2.x|1.x|1.3.2|

**Note:** If you plan to access JSON format data stored in a Cloudera Hadoop cluster, PXF requires a Cloudera version 5.8 or later Hadoop distribution.

