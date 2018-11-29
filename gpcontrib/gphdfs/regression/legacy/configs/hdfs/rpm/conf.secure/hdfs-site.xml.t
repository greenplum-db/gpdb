<?xml version="1.0"?>
<!--
   Licensed to the Apache Software Foundation (ASF) under one or more
 contributor license agreements.  See the NOTICE file distributed with
   this work for additional information regarding copyright ownership.
 The ASF licenses this file to You under the Apache License, Version 2.0
   (the "License"); you may not use this file except in compliance with
 the License.  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
  limitations under the License.
  -->
<?xml-stylesheet type="text/xsl" href="configuration.xsl"?>
<configuration>
<property>
<name>dfs.replication</name>
  <value>1</value>
</property>
  <property>
<name>com.emc.greenplum.gpdb.hdfsconnector.security.user.keytab.file</name>
  <value>/keytab/gpadmin.service.keytab</value>
<description>The location of the keytab file. Greenplum recommends putting the keytable file in the user home directory.</description>
</property>
<property>
  <name>com.emc.greenplum.gpdb.hdfsconnector.security.user.name</name>
<value>gpadmin/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
  <!-- Immediately exit safemode as soon as one DataNode checks in.
  On a multi-node cluster, these configurations must be removed.  -->
  <property>
  <name>dfs.safemode.extension</name>
<value>0</value>
  </property>
<property>
 <name>dfs.safemode.min.datanodes</name>
<value>1</value>
  </property>
<property>
 <name>hadoop.tmp.dir</name>
<value>HADOOP_TMP_DIR/${user.name}</value>
  </property>
<property>
 <name>dfs.namenode.name.dir</name>
<value>file://HADOOP_TMP_DIR/${user.name}/dfs/name</value>
  </property>
<property>
 <name>dfs.namenode.checkpoint.dir</name>
<value>file://HADOOP_TMP_DIR/${user.name}/dfs/namesecondary</value>
  </property>
<property>
 <name>dfs.datanode.data.dir</name>
<value>file://HADOOP_TMP_DIR/${user.name}/dfs/data</value>
  </property>
  <property>
<name>dfs.block.access.token.enable</name>
  <value>true</value>
  </property>
<!-- short circuit reads do not work when security is enabled -->
<property>
  <name>dfs.client.read.shortcircuit</name>
<value>false</value>
</property>
<!-- name node secure configuration info -->
<property>
  <name>dfs.namenode.keytab.file</name>
<value>/keytab/hdfs.service.keytab</value>
</property>
<property>
  <name>dfs.namenode.kerberos.principal</name>
<value>hdfs/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<property>
  <name>dfs.namenode.kerberos.http.principal</name>
<value>HTTP/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<property>
  <name>dfs.namenode.kerberos.internal.spnego.principal</name>
<value>HTTP/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<!-- (optional) secondary name node secure configuration info -->
<property>
  <name>dfs.secondary.namenode.keytab.file</name>
<value>/keytab/hdfs.service.keytab</value>
</property>
<property>
  <name>dfs.secondary.namenode.kerberos.principal</name>
<value>hdfs/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<property>
  <name>dfs.secondary.namenode.kerberos.http.principal</name>
<value>HTTP/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<property>
  <name>dfs.secondary.namenode.kerberos.internal.spnego.principal</name>
<value>HTTP/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<!-- data node secure configuration info -->
<property>
  <name>dfs.datanode.data.dir.perm</name>
<value>700</value>
</property>
<!-- these ports must be set < 1024 for secure operation -->
<!-- conversely they must be set back to > 1024 for non-secure operation -->
<property>
  <name>dfs.datanode.address</name>
<value>0.0.0.0:1004</value>
</property>
<property>
  <name>dfs.datanode.http.address</name>
<value>0.0.0.0:1006</value>
</property>
<!-- remember the principal for the datanode is the principal this hdfs-site.xml file is on -->
<!-- these (next three) need only be set on data nodes -->
<property>
  <name>dfs.datanode.kerberos.principal</name>
<value>hdfs/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<property>
  <name>dfs.datanode.kerberos.http.principal</name>
<value>HTTP/%HOSTNAME%@HD.PIVOTAL.COM</value>
</property>
<property>
  <name>dfs.datanode.keytab.file</name>
<value>/keytab/hdfs.service.keytab</value>
</property>
 <!-- since we included the HTTP principal all keytabs we can use it here -->
<property>
  <name>dfs.web.authentication.kerberos.keytab</name>
<value>/keytab/hdfs.service.keytab</value>
</property>
<property>
  <name>dfs.webhdfs.enabled</name>
  <value>false</value>
</property>
</configuration>