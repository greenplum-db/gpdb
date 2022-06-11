# Enabling gphdfs Authentication with a Kerberos-secured Hadoop Cluster \(Deprecated\) 

Provides steps for configuring Greenplum Database to access external tables in a Hadoop cluster secured with Kerberos.

**Note:** The `gphdfs` external table protocol is deprecated and will be removed in the next major release of Greenplum Database. Consider using the Greenplum Platform Extension Framework \(PXF\) `pxf` external table protocol to access data stored in a Hadoop file system.

Using external tables and the `gphdfs` protocol, Greenplum Database can read files from and write files to a Hadoop File System \(HDFS\). Greenplum segments read and write files in parallel from HDFS for fast performance.

When a Hadoop cluster is secured with Kerberos \("Kerberized"\), Greenplum Database must be configured to allow the Greenplum Database gpadmin role, which owns external tables in HDFS, to authenticate through Kerberos. This topic provides the steps for configuring Greenplum Database to work with a Kerberized HDFS, including verifying and troubleshooting the configuration.

-   [Prerequisites](#topic_gbw_rjl_qr)
-   [Configuring the Greenplum Cluster](#topic_t11_tjl_qr)
-   [Creating and Installing Keytab Files](#topic_wtt_y1r_zr)
-   [Configuring gphdfs for Kerberos](#topic_jsb_cll_qr)
-   [Testing Greenplum Database Access to HDFS](#topic_dlj_yjv_yr)
-   [Troubleshooting HDFS with Kerberos](#topic_mwd_rlm_qr)

**Parent topic:** [Greenplum Database Security Configuration Guide](../topics/preface.html)

## Prerequisites 

Make sure the following components are functioning and accessible on the network:

-   Greenplum Database cluster
-   Kerberos-secured Hadoop cluster. See the *Greenplum Database Release Notes* for supported Hadoop versions.
-   Kerberos Key Distribution Center \(KDC\) server.

## Configuring the Greenplum Cluster 

The hosts in the Greenplum Cluster must have a Java JRE, Hadoop client files, and Kerberos clients installed.

Follow these steps to prepare the Greenplum Cluster.

1.  Install a Java 1.6 or later JRE on all Greenplum cluster hosts.

    Match the JRE version the Hadoop cluster is running. You can find the JRE version by running `java --version` on a Hadoop node.

2.  *\(Optional\)*Confirm that Java Cryptography Extension \(JCE\) is present.

    The default location of the JCE libraries is JAVA\_HOME/lib/security. If a JDK is installed, the directory is JAVA\_HOME/jre/lib/security. The files local\_policy.jar and US\_export\_policy.jar should be present in the JCE directory.

    The Greenplum cluster and the Kerberos server should, preferably, use the same version of the JCE libraries. You can copy the JCE files from the Kerberos server to the Greenplum cluster, if needed.

3.  Set the `JAVA_HOME` environment variable to the location of the JRE in the .bashrc or .bash\_profile file for the `gpadmin` account. For example:

    ```
    export JAVA_HOME=/usr/java/default
    ```

4.  Source the .bashrc or .bash\_profile file to apply the change to your environment. For example:

    ```
    $ source ~/.bashrc
    ```

5.  Install the Kerberos client utilities on all cluster hosts. Ensure the libraries match the version on the KDC server before you install them.

    For example, the following command installs the Kerberos client files on Red Hat or CentOS Linux:

    ```
    $ sudo yum install krb5-libs krb5-workstation
    ```

    Use the `kinit` command to confirm the Kerberos client is installed and correctly configured.

6.  Install Hadoop client files on all hosts in the Greenplum Cluster. Refer to the documentation for your Hadoop distribution for instructions.
7.  Set the Greenplum Database server configuration parameters for Hadoop. The `gp_hadoop_target_version` parameter specifies the version of the Hadoop cluster. See the *Greenplum Database Release Notes* for the target version value that corresponds to your Hadoop distribution. The `gp_hadoop_home` parameter specifies the Hadoop installation directory.

    ```
    $ gpconfig -c gp_hadoop_target_version -v "hdp2"
    $ gpconfig -c gp_hadoop_home -v "/usr/lib/hadoop"
    ```

    See the *Greenplum Database Reference Guide* for more information.

8.  Reload the updated postgresql.conf files for master and segments:

    ```
    gpstop -u
    ```

    You can confirm the changes with the following commands:

    ```
    $ gpconfig -s gp_hadoop_target_version
    $ gpconfig -s gp_hadoop_home
    ```

9.  Grant Greenplum Database gphdfs protocol privileges to roles that own external tables in HDFS, including `gpadmin` and other superuser roles. Grant `SELECT` privileges to enable creating readable external tables in HDFS. Grant `INSERT` privileges to enable creating writable exeternal tables on HDFS.

    ```
    #= GRANT SELECT ON PROTOCOL gphdfs TO gpadmin;
    #= GRANT INSERT ON PROTOCOL gphdfs TO gpadmin;
    ```

10. Grant Greenplum Database external table privileges to external table owner roles:

    ```
    ALTER ROLE <HDFS_USER> CREATEEXTTABLE (type='readable');
    ALTER ROLE <HDFS_USER> CREATEEXTTABLE (type='writable');
    ```

    **Note:** It is best practice to review database privileges, including gphdfs external table privileges, at least annually.


## Creating and Installing Keytab Files 

1.  Log in to the KDC server as root.
2.  Use the `kadmin.local` command to create a new principal for the `gpadmin` user:

    ```
    # kadmin.local -q "addprinc -randkey gpadmin@LOCAL.DOMAIN"
    ```

3.  Use `kadmin.local` to generate a Kerberos service principal for each host in the Greenplum Database cluster. The service principal should be of the form name/role@REALM, where:

    -   *name* is the gphdfs service user name. This example uses `gphdfs`.
    -   *role* is the DNS-resolvable host name of a Greenplum cluster host \(the output of the `hostname -f` command\).
    -   *REALM* is the Kerberos realm, for example `LOCAL.DOMAIN`.
    For example, the following commands add service principals for four Greenplum Database hosts, mdw.example.com, smdw.example.com, sdw1.example.com, and sdw2.example.com:

    ```
    # kadmin.local -q "addprinc -randkey gphdfs/mdw.example.com@LOCAL.DOMAIN"
    # kadmin.local -q "addprinc -randkey gphdfs/smdw.example.com@LOCAL.DOMAIN"
    # kadmin.local -q "addprinc -randkey gphdfs/sdw1.example.com@LOCAL.DOMAIN"
    # kadmin.local -q "addprinc -randkey gphdfs/sdw2.example.com@LOCAL.DOMAIN"
    ```

    Create a principal for each Greenplum cluster host. Use the same principal name and realm, substituting the fully-qualified domain name for each host.

4.  Generate a keytab file for each principal that you created \(`gpadmin` and each `gphdfs` service principal\). You can store the keytab files in any convenient location \(this example uses the directory /etc/security/keytabs\). You will deploy the service principal keytab files to their respective Greenplum host machines in a later step:

    ```
    # kadmin.local -q “xst -k /etc/security/keytabs/gphdfs.service.keytab gpadmin@LOCAL.DOMAIN”
    # kadmin.local -q “xst -k /etc/security/keytabs/mdw.service.keytab gpadmin/mdw gphdfs/mdw.example.com@LOCAL.DOMAIN”
    # kadmin.local -q “xst -k /etc/security/keytabs/smdw.service.keytab gpadmin/smdw gphdfs/smdw.example.com@LOCAL.DOMAIN”
    # kadmin.local -q “xst -k /etc/security/keytabs/sdw1.service.keytab gpadmin/sdw1 gphdfs/sdw1.example.com@LOCAL.DOMAIN”
    # kadmin.local -q “xst -k /etc/security/keytabs/sdw2.service.keytab gpadmin/sdw2 gphdfs/sdw2.example.com@LOCAL.DOMAIN”
    # kadmin.local -q “listprincs”
    ```

5.  Change the ownership and permissions on `gphdfs.service.keytab` as follows:

    ```
    # chown gpadmin:gpadmin /etc/security/keytabs/gphdfs.service.keytab
    # chmod 440 /etc/security/keytabs/gphdfs.service.keytab
    ```

6.  Copy the keytab file for `gpadmin@LOCAL.DOMAIN` to the Greenplum master host:

    ```
    # scp /etc/security/keytabs/gphdfs.service.keytab mdw_fqdn:/home/gpadmin/gphdfs.service.keytab
    ```

7.  Copy the keytab file for each service principal to its respective Greenplum host:

    ```
    # scp /etc/security/keytabs/mdw.service.keytab mdw_fqdn:/home/gpadmin/mdw.service.keytab
    # scp /etc/security/keytabs/smdw.service.keytab smdw_fqdn:/home/gpadmin/smdw.service.keytab
    # scp /etc/security/keytabs/sdw1.service.keytab sdw1_fqdn:/home/gpadmin/sdw1.service.keytab
    # scp /etc/security/keytabs/sdw2.service.keytab sdw2_fqdn:/home/gpadmin/sdw2.service.keytab
    ```


## Configuring gphdfs for Kerberos 

1.  Edit the Hadoop core-site.xml client configuration file on all Greenplum cluster hosts. Enable service-level authorization for Hadoop by setting the `hadoop.security.authorization` property to `true`. For example:

    ```
    <property>
        <name>hadoop.security.authorization</name>
        <value>true</value>
    </property>
    ```

2.  Edit the yarn-site.xml client configuration file on all cluster hosts. Set the resource manager address and yarn Kerberos service principle. For example:

    ```
    <property>
        <name>yarn.resourcemanager.address</name>
        <value><hostname>:<8032></value>
    </property>
    <property>
        <name>yarn.resourcemanager.principal</name>
        <value>yarn/<hostname>@<DOMAIN></value>
    </property>
    ```

3.  Edit the hdfs-site.xml client configuration file on all cluster hosts. Set properties to identify the NameNode Kerberos principals, the location of the Kerberos keytab file, and the principal it is for:

    -   `dfs.namenode.kerberos.principal` - the Kerberos principal name the gphdfs protocol will use for the NameNode, for example `gpadmin@LOCAL.DOMAIN`.
    -   `dfs.namenode.https.principal` - the Kerberos principal name the gphdfs protocol will use for the NameNode's secure HTTP server, for example `gpadmin@LOCAL.DOMAIN`.
    -   `com.emc.greenplum.gpdb.hdfsconnector.security.user.keytab.file` - the path to the keytab file for the Kerberos HDFS service, for example `/home/gpadmin/mdw.service.keytab`. .
    -   `com.emc.greenplum.gpdb.hdfsconnector.security.user.name` - the gphdfs service principal for the host, for example `gphdfs/mdw.example.com@LOCAL.DOMAIN`.
    For example:

    ```
    <property>
        <name>dfs.namenode.kerberos.principal</name>
        <value>gphdfs/gpadmin@LOCAL.DOMAIN</value>
    </property>
    <property>
        <name>dfs.namenode.https.principal</name>
        <value>gphdfs/gpadmin@LOCAL.DOMAIN</value>
    </property>
    <property>
        <name>com.emc.greenplum.gpdb.hdfsconnector.security.user.keytab.file</name>
        <value>/home/gpadmin/gpadmin.hdfs.keytab</value>
    </property>
    <property>
        <name>com.emc.greenplum.gpdb.hdfsconnector.security.user.name</name>
        <value>gpadmin/@LOCAL.DOMAIN</value>
    </property>
    ```


## Testing Greenplum Database Access to HDFS 

Confirm that HDFS is accessible via Kerberos authentication on all hosts in the Greenplum cluster. For example, enter the following command to list an HDFS directory:

```
hdfs dfs -ls hdfs://<namenode>:8020
```

### Create a Readable External Table in HDFS 

Follow these steps to verify that you can create a readable external table in a Kerberized Hadoop cluser.

1.  Create a comma-delimited text file, `test1.txt`, with contents such as the following:

    ```
    25, Bill
    19, Anne
    32, Greg
    27, Gloria
    ```

2.  Persist the sample text file in HDFS:

    ```
    hdfs dfs -put <test1.txt> hdfs://<namenode>:8020/tmp
    ```

3.  Log in to Greenplum Database and create a readable external table that points to the `test1.txt` file in Hadoop:

    ```
    CREATE EXTERNAL TABLE test_hdfs (age int, name text) 
    LOCATION('gphdfs://<namenode>:<8020>/tmp/test1.txt') 
    FORMAT 'text' (delimiter ',');
    ```

4.  Read data from the external table:

    ```
    SELECT * FROM test_hdfs;
    ```


### Create a Writable External Table in HDFS 

Follow these steps to verify that you can create a writable external table in a Kerberized Hadoop cluster. The steps use the `test_hdfs` readable external table created previously.

1.  Log in to Greenplum Database and create a writable external table pointing to a text file in HDFS:

    ```
    CREATE WRITABLE EXTERNAL TABLE test_hdfs2 (LIKE test_hdfs) 
    LOCATION ('gphdfs://<namenode>:8020/tmp/test2.txt'
    FORMAT 'text' (DELIMITER ',');
    ```

2.  Load data into the writable external table:

    ```
    INSERT INTO test_hdfs2 
    SELECT * FROM test_hdfs;
    ```

3.  Check that the file exists in HDFS:

    ```
    hdfs dfs -ls hdfs://<namenode>:8020/tmp/test2.txt
    ```

4.  Verify the contents of the external file:

    ```
    hdfs dfs -cat hdfs://<namenode>:8020/tmp/test2.txt
    ```


## Troubleshooting HDFS with Kerberos 

### Forcing Classpaths 

If you encounter "class not found" errors when executing `SELECT` statements from `gphdfs` external tables, edit the $GPHOME/lib/hadoop-env.sh file and add the following lines towards the end of the file, before the `JAVA_LIBRARY_PATH` is set. Update the script on all of the cluster hosts.

```
if [ -d "/usr/hdp/current" ]; then
for f in /usr/hdp/current/**/*.jar; do
    CLASSPATH=${CLASSPATH}:$f;
done
fi
```

### Enabling Kerberos Client Debug Messages 

To see debug messages from the Kerberos client, edit the $GPHOME/lib/hadoop-env.sh client shell script on all cluster hosts and set the `HADOOP_OPTS` variable as follows:

```
export HADOOP_OPTS="-Djava.net.prefIPv4Stack=true -Dsun.security.krb5.debug=true ${HADOOP_OPTS}"
```

### Adjusting JVM Process Memory on Segment Hosts 

Each segment launches a JVM process when reading or writing an external table in HDFS. To change the amount of memory allocated to each JVM process, configure the `GP_JAVA_OPT` environment variable.

Edit the $GPHOME/lib/hadoop-env.sh client shell script on all cluster hosts.

For example:

```
export GP_JAVA_OPT=-Xmx1000m
```

### Verify Kerberos Security Settings 

Review the /etc/krb5.conf file:

-   If AES256 encryption is not disabled, ensure that all cluster hosts have the JCE Unlimited Strength Jurisdiction Policy Files installed.
-   Ensure all encryption types in the Kerberos keytab file match definitions in the krb5.conf file.

    ```
    cat /etc/krb5.conf | egrep supported_enctypes
    ```


### Test Connectivity on an Individual Segment Host 

Follow these steps to test that a single Greenplum Database host can read HDFS data. This test method executes the Greenplum `HDFSReader` Java class at the command-line, and can help to troubleshoot connectivity problems outside of the database.

1.  Save a sample data file in HDFS.

    ```
    hdfs dfs -put test1.txt hdfs://<namenode>:8020/tmp
    ```

2.  On the segment host to be tested, create an environment script, `env.sh`, like the following:

    ```
    export JAVA_HOME=/usr/java/default
    export HADOOP_HOME=/usr/lib/hadoop
    export GP_HADOOP_CON_VERSION=hdp2
    export GP_HADOOP_CON_JARDIR=/usr/lib/hadoop
    ```

3.  Source all environment scripts:

    ```
    source /usr/local/greenplum-db/greenplum_path.sh
    source env.sh
    source $GPHOME/lib/hadoop-env.sh
    ```

4.  Test the Greenplum Database HDFS reader:

    ```
    java com.emc.greenplum.gpdb.hdfsconnector.HDFSReader 0 32 TEXT hdp2 gphdfs://<namenode>:8020/tmp/test1.txt
    ```


