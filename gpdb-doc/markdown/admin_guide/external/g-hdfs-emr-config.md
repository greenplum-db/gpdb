# Using Amazon EMR with Greenplum Database installed on AWS \(Deprecated\) 

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database.

Amazon Elastic MapReduce \(EMR\) is a managed cluster platform that can run big data frameworks, such as Apache Hadoop and Apache Spark, on Amazon Web Services \(AWS\) to process and analyze data. For a Greenplum Database system that is installed on Amazon Web Services \(AWS\), you can define Greenplum Database external tables that use the `gphdfs` protocol to access files on an Amazon EMR instance HDFS.

In addition to the steps described in [One-time gphdfs Protocol Installation \(Deprecated\)](g-one-time-hdfs-protocol-installation.html), you must also ensure Greenplum Database can access the EMR instance. If your Greenplum Database system is running on an Amazon Elastic Compute Cloud \(EC2\) instance, you configure the Greenplum Database system and the EMR security group.

For information about Amazon EMR, see [https://aws.amazon.com/emr/](https://aws.amazon.com/emr/). For information about Amazon EC2, see [https://aws.amazon.com/ec2/](https://aws.amazon.com/ec2/)

## Configuring Greenplum Database and Amazon EMR 

These steps describe how to set up Greenplum Database system and an Amazon EMR instance to support Greenplum Database external tables:

1.  Ensure that the appropriate Java \(including JDK\) and Hadoop environments are correctly installed on all Greenplum Database segment hosts.

    For example, Amazon EMR Release 4.0.0 includes Apache Hadoop 2.6.0. This Amazon page describes [Amazon EMR Release 4.0.0](https://aws.amazon.com/about-aws/whats-new/2015/07/amazon-emr-release-4-0-0-with-new-versions-of-apache-hadoop-hive-and-spark-now-available/).

    For information about Hadoop versions used by EMR and Greenplum Database, see [Table 1](#table_at3_czf_ht).

2.  Ensure the environment variables and Greenplum Database server configuration parameters are set:
    -   System environment variables:
        -   `HADOOP_HOME`
        -   `JAVA_HOME`
    -   Greenplum Database server configuration parameters:
        -   `gp_hadoop_target_version`
        -   `gp_hadoop_home`
3.  Configure communication between Greenplum Database and the EMR instance Hadoop master.

    For example, open port 8020 in the AWS security group.

4.  Configure for communication between Greenplum Database and EMR instance Hadoop data nodes. Open a TCP/IP port for so that Greenplum Database segments hosts can communicate with EMR instance Hadoop data nodes.

    For example, open port 50010 in the AWS security manager.


This table lists EMR and Hadooop version information that can be used to configure Greenplum Database.

|EMR Version|EMR Apache Hadoop Version|EMR Hadoop Master Port|gp\_hadoop\_target\_version|Hadoop Version on Greenplum Database Segment Hosts|
|-----------|-------------------------|----------------------|---------------------------|--------------------------------------------------|
|4.0|2.6|8020|`hadoop2`|Apache Hadoop 2.x|
|3.9|2.4|9000|`hadoop2`|Apache Hadoop 2.x|
|3.8|2.4|9000|`hadoop2`|Apache Hadoop 2.x|
|3.3|2.4|9000|`hadoop2`|Apache Hadoop 2.x|

**Parent topic:** [Accessing HDFS Data with gphdfs \(Deprecated\)](../external/g-using-hadoop-distributed-file-system--hdfs--tables.html)

