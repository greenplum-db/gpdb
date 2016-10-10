
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
 <name>yarn.nodemanager.aux-services</name>
<value>mapreduce%SEPARATOR%shuffle</value>
 </property>
   <property>
 <name>yarn.nodemanager.aux-services.mapreduce%SEPARATOR%shuffle.class</name>
  <value>org.apache.hadoop.mapred.ShuffleHandler</value>
  </property>
<property>
  <name>yarn.log-aggregation-enable</name>
  <value>false</value>
  </property>
  <property>
<description>List of directories to store localized files in.</description>
  <name>yarn.nodemanager.local-dirs</name>
  <value>YARN_TMP_DIR/${user.name}/nm-local-dir</value>
  </property>
  <property>
<description>Where to store container logs.</description>
  <name>yarn.nodemanager.log-dirs</name>
  <value>YARN_LOG_DIR/containers</value>
  </property>
   <property>
  <description>Default time (in seconds) to retain log files on the NodeManager Only applicable if log-aggregation is disabled. Keeping it to 5 days</description>
  <name>yarn.nodemanager.log.retain-seconds</name>
  <value>432000</value>
   </property>
   <property>
 <description>The address for the web proxy as HOST:PORT, if this is not
given or if it matches yarn.resourcemanager.address then the proxy will
   run as part of the RM</description>
 <name>yarn.web-proxy.address</name>
 <value>0.0.0.0:9999</value>
 </property>
 <property>
   <description>Classpath for typical applications.</description>
   <name>yarn.application.classpath</name>
    <value>%YARN_APPLICATION_CLASSPATH%</value>
 </property>
<property>
  <name>yarn.nodemanager.container-executor.class</name>
<value>org.apache.hadoop.yarn.server.nodemanager.DefaultContainerExecutor</value>
</property>
<property>
  <name>yarn.nodemanager.linux-container-executor.group</name>
<value>yarn</value>
</property>
</configuration>
