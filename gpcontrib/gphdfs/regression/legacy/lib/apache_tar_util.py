"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import os
import re
import tinctest

from rpm_util import RPMUtil
from tinctest.lib import local_path, run_shell_command

from hadoop_util import HadoopUtil

class ApacheTarUtil(HadoopUtil):
    """Utility for installing PHD Single node clusters using RPMs"""
    
    def __init__(
                    self, hadoop_src_artifact_url, hadoop_install_dir, hadoop_data_dir, template_conf_dir, hostname = 'localhost',
                    secure_hadoop = False
                ):
        HadoopUtil.__init__(self, hadoop_src_artifact_url, hadoop_install_dir, hadoop_data_dir, hostname)
        self.rpmutil = RPMUtil()
        self.hostname = hostname
        # we build the hadoop binaries from the source code so as to avoid any issues
        self.hadoop_src_artifact_url = hadoop_src_artifact_url
        self.hadoop_install_dir = hadoop_install_dir
        self.hadoop_binary_loc = ''
        self.template_conf_dir = template_conf_dir
        self.secure_hadoop = secure_hadoop
        # Constants
        # under the hadoop template configuration directory
        # both the below directories should be present
        self.SECURE_DIR_NAME = "conf.secure"        # secure configuration files location   
        self.NON_SECURE_DIR_NAME = "conf.pseudo"    # non-secure configuration files location
                    
    def cleanup(self):
        """
        Clean-up process to:
        1. kill all the hadoop daemon process from previous runs if any
        2. Remove the contents from the hadoop installation & configuration locations
        """
        self.stop_hadoop()
        cmd_str = "ps aux | awk '/\-Dhadoop/{print $2}' | xargs sudo kill -9"
        run_shell_command(cmd_str, "Kill zombie hadoop daemons")
        cmd_str = "sudo rm -rf "
        for key,value in self.HADOOP_ENVS.iteritems():
            cmd_str = cmd_str + value +"* "
        cmd_str = cmd_str + "/etc/gphd"
        run_shell_command(cmd_str,"Clean up HDFS files")
        
    def install_binary(self):
        """
        Copy the hadoop binary into /usr/lib/hadoop
        """
        # create hadoop_home directory
        run_shell_command("sudo rm -rf %s; sudo mkdir %s" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_HOME']))
        # copy the hadoop binaries in hadoop_home
        run_shell_command("sudo cp -r %s/* %s" %(self.hadoop_binary_loc, self.HADOOP_ENVS['HADOOP_HOME']))
        # create hadoop users
        self.create_hadoop_users()
        
    def install_hadoop_configurations(self):
        """
        Based on type of installation secure or non-secure, 
        installs the updated template configuration files
        and makes required changes to the env files.
        """
        # copy the hadoop conf directory under /etc/hadoop/conf
        run_shell_command("sudo rm -rf /etc/hadoop ; sudo mkdir -p %s" %self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        run_shell_command("sudo cp %s/etc/hadoop/* %s" %(self.hadoop_binary_loc, self.HADOOP_ENVS['HADOOP_CONF_DIR']))
        run_shell_command("sudo rm -rf %s/etc/hadoop" %self.HADOOP_ENVS['HADOOP_HOME'])
        run_shell_command("cd %s/etc/; sudo ln -s %s hadoop" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR']))
        text =  "export HADOOP_HOME=%s\n"   \
                "export JAVA_HOME=%s\n" \
                "export HADOOP_OPTS=\"-Djava.net.preferIPv4Stack=true " \
                "-Djava.library.path=$HADOOP_HOME/lib/native/\"\n"  \
                "export HADOOP_LOG_DIR=%s\n"    \
                "export YARN_LOG_DIR=%s\n"  \
                "export HADOOP_MAPRED_LOG_DIR=%s\n" %(  self.HADOOP_ENVS['HADOOP_HOME'], 
                                                        self.get_java_home(), 
                                                        self.HADOOP_ENVS['HADOOP_LOG_DIR'], 
                                                        self.HADOOP_ENVS['YARN_LOG_DIR'],
                                                        self.HADOOP_ENVS['MAPRED_LOG_DIR']
                                                    )
        hadoop_env_file = os.path.join( self.HADOOP_ENVS['HADOOP_CONF_DIR'], "hadoop-env.sh" )
        self.give_others_write_perm(hadoop_env_file)
        self.append_text_to_file(hadoop_env_file,text)
        
        # check the type of hadoop installation - secure or non secure
        if self.secure_hadoop:
            # SECURE_DIR_NAME is expected to be present under template configuration directory
            secure_conf = os.path.join(self.template_conf_dir, self.SECURE_DIR_NAME)
            super(ApacheTarUtil,self).install_hadoop_configurations(secure_conf, self.HADOOP_ENVS['HADOOP_CONF_DIR'], "apache")
            # update hadoop-env.sh file
            #
            text =  "\n### Added env variables\n" \
                    "export HADOOP_SECURE_DN_USER=hdfs\n"  \
                    "export JSVC_HOME=/usr/bin\n"  \
                    "export HADOOP_SECURE_DN_LOG_DIR=${HADOOP_LOG_DIR}/hdfs\n" \
                    "export HADOOP_PID_DIR=/var/run/hadoop-hdfs/\n"    \
                    "export HADOOP_SECURE_DN_PID_DIR=${HADOOP_PID_DIR}\n"
                    
            self.append_text_to_file(hadoop_env_file,text)
            
            
            # change the permissions of container-executor
            container_bin_path = os.path.join(self.HADOOP_ENVS['YARN_HOME'],'bin/container-executor')
            cmd_str = "sudo chown root:yarn %s" %container_bin_path
            run_shell_command(cmd_str)
            cmd_str = "sudo chmod 050 %s" %container_bin_path
            run_shell_command(cmd_str)
            cmd_str = "sudo chmod u+s %s" %container_bin_path
            run_shell_command(cmd_str)
            cmd_str = "sudo chmod g+s %s" %container_bin_path
            run_shell_command(cmd_str) 
        else:
            # NON_SECURE_DIR_NAME is expected to be present under template configuration directory
            non_secure_conf = os.path.join(self.template_conf_dir, self.NON_SECURE_DIR_NAME)
            super(ApacheTarUtil, self).install_hadoop_configurations(non_secure_conf, self.HADOOP_ENVS['HADOOP_CONF_DIR'], "apache")
        # revert back to old permissions
        self.remove_others_write_perm(hadoop_env_file)
            
    def start_hdfs(self):
        # format namenode
        cmd_str = "sudo -u hdfs %s/bin/hdfs --config %s namenode -format" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        namenode_formatted = run_shell_command(cmd_str)
        if not namenode_formatted:
            raise Exception("Exception in namnode formatting")
        
        # start namenode
        cmd_str = "sudo -u hdfs %s/sbin/hadoop-daemon.sh --config %s start namenode" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        namenode_started = run_shell_command(cmd_str)
        if not namenode_started:
            raise Exception("Namenode not started")
        
        
        cmd_str = "sudo %s/sbin/hadoop-daemon.sh --config %s start datanode" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        namenode_started = run_shell_command(cmd_str)
        if not namenode_started:
            raise Exception("Namenode not started")
            
        cmd_str = "sudo -u hdfs %s/sbin/hadoop-daemon.sh --config %s start secondarynamenode" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        namenode_started = run_shell_command(cmd_str)
        if not namenode_started:
            raise Exception("Secondary namenode not started")

    def set_hdfs_permissions(self):
        if self.secure_hadoop:
            hdfs_cmd = "sudo %s/bin/hdfs dfs" %self.HADOOP_ENVS['HADOOP_HOME']
        else:
            hdfs_cmd = "sudo -u hdfs %s/bin/hdfs dfs" %self.HADOOP_ENVS['HADOOP_HOME']
        # set hdfs permissions
        cmd_str = "%s -chmod -R 777 /" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -mkdir /tmp" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -chmod 1777 /tmp" %hdfs_cmd
        run_shell_command(cmd_str)
        # cmd_str = "%s -mkdir -p /var/log/hadoop-yarn" %hdfs_cmd
        # run_shell_command(cmd_str)
        cmd_str = "%s -mkdir /user" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -chmod 777 /user" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -mkdir /user/history" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -chown mapred:hadoop /user/history" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -chmod 1777 -R /user/history" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -ls -R /" %hdfs_cmd
        run_shell_command(cmd_str)
        
    def start_yarn(self):
        # start yarn daemons
        # start resource manager
        self.set_hdfs_permissions()
        cmd_str = "sudo %s/sbin/yarn-daemon.sh --config %s start resourcemanager" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        namenode_started = run_shell_command(cmd_str)
        if not namenode_started:
            raise Exception("Resource manager not started")
        
        # start node manager
        cmd_str = "sudo %s/sbin/yarn-daemon.sh --config %s start nodemanager" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        namenode_started = run_shell_command(cmd_str)
        if not namenode_started:
            raise Exception("Node manager not started")
        
        # start history server
        cmd_str = "sudo %s/sbin/mr-jobhistory-daemon.sh --config %s start historyserver" %(self.HADOOP_ENVS['HADOOP_HOME'], self.HADOOP_ENVS['HADOOP_CONF_DIR'])
        namenode_started = run_shell_command(cmd_str)
        if not namenode_started:
            raise Exception("History server not started")
            
            
    def start_hadoop(self):
        """
        Starts the PHD cluster and checks the JPS status
        """
        self.start_hdfs()
        self.start_yarn()
        res = {}
        # run jps command & check for hadoop daemons
        cmd_str = "sudo jps"
        run_shell_command(cmd_str, "Check Hadoop Daemons", res)
        result = res['stdout']
        tinctest.logger.info("\n**** Following Hadoop Daemons started **** \n%s" %result)
        tinctest.logger.info("*** Hadoop Started Successfully!!")
        
    def stop_hadoop(self):
        """
        Stops the PHD cluster
        """
        hadoop_home = self.HADOOP_ENVS['HADOOP_HOME']
        run_shell_command("sudo  %s/sbin/mr-jobhistory-daemon.sh stop historyserver" %hadoop_home, "Stop history-server")
        run_shell_command("sudo %s/sbin/yarn-daemon.sh stop nodemanager" %hadoop_home, "Stop Node manager")
        run_shell_command("sudo  %s/sbin/yarn-daemon.sh stop resourcemanager" %hadoop_home, "Stop resourcemanager")
        run_shell_command("sudo %s/sbin/hadoop-daemon.sh stop datanode" %hadoop_home, "Stop datanode")
        run_shell_command("sudo  %s/sbin/hadoop-daemon.sh stop secondarynamenode" %hadoop_home, "Stop secondarynamenode")
        run_shell_command("sudo  %s/sbin/hadoop-daemon.sh stop namenode" %hadoop_home, "Stop namenode")

    def get_hadoop_env(self):
        """
        Returns a dictionary of hadoop environment variables like:
        1. HADOOP_HOME
        2. HADOOP_CONF_DIR
        3. HADOOP_COMMON_HOME
        4. HADOOP_HDFS_HOME
        5. YARN_HOME
        6. HADOOP_MAPRED_HOME
        """
        return self.HADOOP_ENVS
        
        
    def init_cluster(self):
        """
        Init point for starting up the PHD cluster
        """
        self.download_binary_and_untar()
        self.build_hadoop_from_source()
        self.install_jsvc()
        self.cleanup()
        self.install_binary()
        self.install_hadoop_configurations()
        self.start_hadoop()
        

if __name__ == '__main__':
    hdfs_util = ApacheTarUtil("http://build-prod.sanmateo.greenplum.com/releases/pivotal-hd/1.1.1/PHD-1.1.1.0-82.tar.gz","/data/gpadmin/hadoop_test","../configs/rpm/","localhost")
    hdfs_util.init_cluster()
    
