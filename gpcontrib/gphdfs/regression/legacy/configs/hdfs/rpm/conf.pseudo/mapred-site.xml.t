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
<name>mapred.job.tracker</name>
  <value>%HOSTNAME%:8021</value>
</property>
  <property>
  <name>mapreduce.framework.name</name>
<value>yarn</value>
  </property>
<property>
<description>To set the value of tmp directory for map and reduce tasks.</description>
<name>mapreduce.task.tmp.dir</name>
  <value>MAPRED_TMP_DIR/${user.name}/tasks</value>
</property>
  <property>
   <name>yarn.app.mapreduce.am.staging-dir</name>
  <value>/user</value>
</property>
  <property>
   <name>mapreduce.map.memory.mb</name>
  <value>1536</value>
</property>
  <property>
   <name>mapreduce.map.java.opts</name>
  <value>-Xmx1024M</value>
</property>
  <property>
   <name>mapreduce.reduce.memory.mb</name>
  <value>3072</value>
</property>
  <property>
   <name>mapreduce.reduce.java.opts</name>
  <value>-Xmx2560M</value>
</property>
  <property>
   <name>mapreduce.task.io.sort.mb</name>
  <value>512</value>
</property>
  <property>
   <name>mapreduce.task.io.sort.factor</name>
  <value>100</value>
</property>
  <property>
   <name>mapreduce.reduce.shuffle.parallelcopies</name>
  <value>50</value>
</property>
</configuration>