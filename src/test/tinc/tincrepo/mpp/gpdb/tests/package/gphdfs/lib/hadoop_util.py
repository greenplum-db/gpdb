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
from tinctest.main import TINCException

class HadoopUtilException(TINCException):
    """Exception class for Kerberos util"""
    pass

class HadoopUtil(object):
    """Utility for installating single-node Hadoop cluster"""

    def __init__(self, hadoop_artifact_url, hadoop_install_dir, hadoop_data_dir, hostname = "localhost"):
        self.hadoop_artifact_url = hadoop_artifact_url
        self.hadoop_install_dir = hadoop_install_dir
        self.hadoop_binary_loc = ''
        self.hadoop_data_dir = hadoop_data_dir
        self.hostname = hostname
        self.HADOOP_ENVS = {
                                "HADOOP_HOME" : "/usr/lib/hadoop/",
                                "HADOOP_COMMON_HOME" : "/usr/lib/hadoop/share/hadoop/",
                                "HADOOP_HDFS_HOME" : "/usr/lib/hadoop/",
                                "HADOOP_MAPRED_HOME" : "/usr/lib/hadoop/",
                                "YARN_HOME" : "/usr/lib/hadoop/",
                                "HADOOP_CONF_DIR" : "/etc/hadoop/conf",
                                "HADOOP_TMP_DIR" : "%s/hadoop-hdfs/cache/" %self.hadoop_data_dir,
                                "MAPRED_TMP_DIR" : "%s/hadoop-mapreduce/cache/" %self.hadoop_data_dir,
                                "YARN_TMP_DIR" : "%s/hadoop-yarn/cache/" %self.hadoop_data_dir,
                                "HADOOP_LOG_DIR" : "%s/hadoop-logs/hadoop-hdfs" %self.hadoop_data_dir,
                                "MAPRED_LOG_DIR" : "%s/hadoop-logs/hadoop-mapreduce" %self.hadoop_data_dir,
                                "YARN_LOG_DIR" : "%s/hadoop-logs/hadoop-yarn" %self.hadoop_data_dir 
                            }
        self.MVN_REPO_URL = "http://repos.fedorapeople.org/repos/dchen/apache-maven/epel-apache-maven.repo"
        self.MVN_REPO_LOC = "/etc/yum.repos.d/epel-apache-maven.repo"
        self.PROTOC_BUFFER_URL = "http://protobuf.googlecode.com/files/protobuf-2.5.0.tar.gz"
        self.JSVC_SRC_URL = "http://apache.petsads.us//commons/daemon/source/commons-daemon-1.0.15-src.zip"
        
    def create_hadoop_users(self):
        """
        Creates the following hadoop users & groups:
        1. hadoop
        2. hdfs
        3. yarn
        4. mapred
        """
        # we are creating user & groups here
        # we are bound to see errors if users already exist, and thats fine
        run_shell_command("sudo groupadd hadoop")
        run_shell_command("sudo groupadd yarn")
        run_shell_command("sudo useradd yarn -g yarn -M")
        run_shell_command("sudo usermod -a -G hadoop yarn")
        run_shell_command("sudo useradd hdfs -g hadoop -M")
        run_shell_command("sudo useradd mapred -g hadoop -M")
        run_shell_command("sudo useradd hadoop -g hadoop -M")

    def download_binary_and_untar(self):
        """
        1. Downloads the hadoop binary
        2. Untars the binary into the specified installation location
        """
        # check if the installation exists or not
        # delete its contents if exist else create a new one
        if os.path.isdir(self.hadoop_install_dir):
            cmd_str = "sudo rm -rf %s/*" %self.hadoop_install_dir
        else:
            cmd_str = "mkdir -p %s" %self.hadoop_install_dir
        run_shell_command(cmd_str,"Check Hadoop install directory")
        res = {'rc':0, 'stdout':'', 'stderr':''}
        run_shell_command("basename %s" %self.hadoop_artifact_url, "To get binary name", res)
        binary_name = res['stdout'].split('\n')[0]
        tinctest.logger.debug("Hadoop Binary - %s" %binary_name)
        binary_path = os.path.join(self.hadoop_install_dir, binary_name)
        tinctest.logger.debug("Hadoop Binary Path - %s" %binary_path)
        cmd_str = "wget -O %s %s" %(binary_path, self.hadoop_artifact_url)
        res = {}
        result = run_shell_command(cmd_str, "Download binary", res)
        if not result:
            raise Exception("Failed to download hadoop binary: %s" %res['stderr'])
        res = {}
        result = run_shell_command(
                                    "tar -xzf %s -C %s" %(binary_path, self.hadoop_install_dir),
                                    "Untar binary",
                                     res
                                   )
        if not result:
            raise Exception("Failed to untar hadoop binary: %s" %res['stderr'])
        # removing the .tar.gz to get the installation directory
        self.hadoop_binary_loc = binary_path[:-7]
        tinctest.logger.debug("Hadoop Installation Path - %s" %self.hadoop_binary_loc)
        
    def install_binary(self):
        ##TODO: write a generic method for installing hadoop
        pass

    def install_jsvc(self):
        """
        Builds the jsvc binary from source code and installs it in /usr/bin/jsvc 
        """
        jsvc_src_url = os.getenv("JSVC_SRC_URL", self.JSVC_SRC_URL)
        res = {}
        cmd_str = "basename %s" %jsvc_src_url
        run_shell_command(cmd_str, "Get basenmae of JSVC_SRC_URL", res)
        basename = res['stdout'].splitlines()[0]
        tinctest.logger.debug("JSVC binary - %s" %basename)
        res = {}
        jsvc_src_path = os.path.join(self.hadoop_install_dir, basename)
        cmd_str = "wget -O %s %s" %(jsvc_src_path, jsvc_src_url)
        result = run_shell_command(cmd_str, "Download the JSVC source code", res)
        if not result:
            raise HadoopUtilException("Failed to download JSVC binary : %s" %res['stderr'])
        # unzip the binary
        cmd_str = "unzip -d %s %s" %(self.hadoop_install_dir, jsvc_src_path)
        run_shell_command(cmd_str, "Unzip jsvc binary")
        jsvc_dir = jsvc_src_path[:-4]
        tinctest.logger.debug("Jsvc src directory - %s" %jsvc_dir)
        # change to jsvc installation directory and configure
        cmd_str = "cd %s/src/native/unix; export CFLAGS=-m64; export LDFLAGS=-m64;  " \
                  " sudo ./configure --with-java=$JAVA_HOME" %jsvc_dir
        run_shell_command(cmd_str, "cd to jsvc installation")
        # make
        cmd_str = "cd %s/src/native/unix; export CFLAGS=-m64; export LDFLAGS=-m64;  " \
                  "sudo make" %jsvc_dir
        run_shell_command(cmd_str, "make the jsvc binary")
        jsvc_binary = "%s/src/native/unix/jsvc" %jsvc_dir
        # If the jsvc binary is created successfully, copy it to /usr/bin; else raise Exception
        if os.path.exists(jsvc_binary):
            run_shell_command("sudo cp %s /usr/bin/jsvc" %jsvc_binary, "Copy the jsvc binary to usr/bin")
        else:
            raise HadoopUtilException("jsvc binary not found!!")
            
        
        
    def _perform_regex_transformations(self, config_dir, file_type, transforms):
        """
        Performs substitions in the files from the given dir location
        """
        files = [ file for file in os.listdir(local_path(config_dir)) if file.endswith(file_type) ]
        for file in files:
            input_file_path = local_path(config_dir + "/" + file)
            output_filename = file.replace('.t','')
            output_file_path = local_path(config_dir + "/" + output_filename)
            with open(input_file_path, 'r') as input:
                with open(output_file_path, 'w') as output:
                    for line in input.readlines():
                        for key,value in transforms.iteritems():
                            line = re.sub(key,value,line)
                        output.write(line)
                        
    def is_java6(self, java_home):
        res = {}
        if java_home.find('bin') >=0 :
            cmd_str = java_home + "/java -version"
        else:
            cmd_str = java_home + "/bin/java -version"
        run_shell_command(cmd_str, "check java version", res)
        if res['stderr'].splitlines()[0].find("java version \"1.6") == 0:
            return True
        else:
            return False
                        
    def set_java7_home(self):
        """
        Downloads jdk7 and sets JAVA_HOME to jdk7
        """
        java_home = "/opt/jdk7"
        # download jdk7
        cmd_str =   "wget -O jdk7.tar.gz --no-check-certificate --no-cookies --header "   \
                    "\"Cookie: oraclelicense=accept-securebackup-cookie\" "  \
                    "http://download.oracle.com/otn-pub/java/jdk/7u60-b19/jdk-7u60-linux-x64.tar.gz"
        if not run_shell_command(cmd_str, "Download jdk 7"):
            raise HadoopUtilException("Couldn't download jdk 7")
        run_shell_command("sudo rm -rf %s; sudo mkdir -p %s" %(java_home, java_home), "Create java home dir")
        cmd_str = "sudo tar -xzf jdk7.tar.gz -C %s" %java_home
        run_shell_command(cmd_str, "Install jdk 7")
        res = {}
        run_shell_command("sudo ls %s" %java_home, "Get actual java home path", res)
        java_home = java_home + '/' + res['stdout'].splitlines()[0]
        # install JCE cryptography files
        cmd_str =   "wget -O jce.zip --no-check-certificate --no-cookies --header " \
                    "\"Cookie: oraclelicense=accept-securebackup-cookie\" "  \
                    "http://download.oracle.com/otn-pub/java/jce/7/UnlimitedJCEPolicyJDK7.zip"
        if not run_shell_command(cmd_str, "Download JCE policy files"):
            raise HadoopUtilException("Couldn't download JCE policy files")
        run_shell_command("rm -rf UnlimitedJCEPolicy; unzip jce.zip")
        cmd_str = "sudo cp UnlimitedJCEPolicy/*.jar %s/jre/lib/security/" %java_home
        run_shell_command(cmd_str, "Install JCE files")
        # update /etc/profile
        cmd_str = "sudo sed -i 's:JAVA_HOME=/opt/jdk.*:JAVA_HOME=%s/:' /etc/profile" %java_home
        if not run_shell_command(cmd_str, "Update /etc/profile with java7 home"):
            raise HadoopUtilException("Couldn't update java7 home in /etc/profile")
        return java_home
        
    def get_java_home(self):
        """
        Returns java home is set. else throws exception
        
        @return: JAVA_HOME
        """
        java_home = os.getenv("JAVA_HOME")
        tinctest.logger.info("JAVA_HOME - %s" %java_home)
        if java_home is None:
            return self.set_java7_home()
        elif self.is_java6(java_home):
            return self.set_java7_home()
        else:
            return java_home
                        
    def append_text_to_file(self, file_name, text_to_append):
        with open(file_name, "a") as append_file:
            append_file.write(text_to_append)    
    
    def _update_hadoop_conf_template_files(self, transforms, template_dir):
        tinctest.logger.debug("Hadoop configuration template dir : %s" %template_dir)
        self._perform_regex_transformations(template_dir, ".t", transforms)

    def give_others_write_perm(self, file):
        run_shell_command("sudo chmod o+w %s" %file)
        
    def remove_others_write_perm(self, file):
        run_shell_command("sudo chmod o-w %s" %file)
        
    def get_hadoop_version(self):
        cmd_str = "find -L %s -name hadoop | egrep 'bin/hadoop'" %self.HADOOP_ENVS['HADOOP_HOME']
        res= {}
        run_shell_command(cmd_str, "Find hadoop binary", res)
        hadoop_bin = res['stdout'].splitlines()[0]
        cmd_str = "%s version" %hadoop_bin
        res= {}
        run_shell_command(cmd_str, "Get hadoop version", res)
        hadoop_version = res['stdout'].splitlines()[0].split('-')[0]
        tinctest.logger.info("Hadoop version - %s" %hadoop_version)
        return hadoop_version
        
    def create_data_directories(self):
        # Create data directories for HDFS, YARN & MAPREDUCE
        cmd_str = "sudo mkdir -p %s %s %s"  %(  self.HADOOP_ENVS['HADOOP_TMP_DIR'],
                                                self.HADOOP_ENVS['MAPRED_TMP_DIR'],
                                                self.HADOOP_ENVS['YARN_TMP_DIR']
                                              )
        run_shell_command(cmd_str, "Create data directories for hdfs, yarn and mapreduce")
        # Give required permissions to hdfs data directories
        run_shell_command("sudo chown -R hdfs:hadoop %s/hadoop-hdfs" %self.hadoop_data_dir)
        run_shell_command("sudo chmod 775 %s/hadoop-hdfs" %self.hadoop_data_dir)
        run_shell_command("sudo chmod 777 %s" %self.HADOOP_ENVS['HADOOP_TMP_DIR'])
        # Give required permissions to mapred data directories
        run_shell_command("sudo chown -R mapred:hadoop %s/hadoop-mapreduce" %self.hadoop_data_dir)
        run_shell_command("sudo chmod 775 %s/hadoop-mapreduce" %self.hadoop_data_dir)
        run_shell_command("sudo chmod 777 %s" %self.HADOOP_ENVS['MAPRED_TMP_DIR'])
        # Give required permissions to yarn data directories
        run_shell_command("sudo chown -R yarn:hadoop %s/hadoop-yarn" %self.hadoop_data_dir)
        run_shell_command("sudo chmod 775 %s/hadoop-yarn" %self.hadoop_data_dir)
        run_shell_command("sudo chmod 777 %s" %self.HADOOP_ENVS['YARN_TMP_DIR'])
        
    def create_log_directories(self):
        # Create log directories
        cmd_str = "sudo mkdir -p %s %s %s" %(  self.HADOOP_ENVS['HADOOP_LOG_DIR'],
                                                self.HADOOP_ENVS['MAPRED_LOG_DIR'],
                                                self.HADOOP_ENVS['YARN_LOG_DIR']
                                              )
        run_shell_command(cmd_str, "Create log directories")                                      
        run_shell_command("sudo chmod 775 %s/hadoop-logs/*" %self.hadoop_data_dir)
        run_shell_command("sudo chgrp hadoop %s/hadoop-logs/*" %self.hadoop_data_dir)
        run_shell_command("sudo chown hadoop %s" %self.HADOOP_ENVS['HADOOP_LOG_DIR'])
        run_shell_command("sudo chown mapred %s" %self.HADOOP_ENVS['MAPRED_LOG_DIR'])
        run_shell_command("sudo chown yarn %s" %self.HADOOP_ENVS['YARN_LOG_DIR'])
        
    def install_hadoop_configurations(self, template_conf_dir, hadoop_conf_dir, hadoop_type = None):
        """
        Copies the updated template configuration files into the the 
        hadoop configuration directory
        
        @param template_conf_dir: directory where template hadoop configuration files are located
        @param hadoop_conf_dir: HADOOP_CONF_DIR where the updated template files are copied to
        """
        yarn_app_path = "\n"
        if hadoop_type == "apache":
            yarn_app_path = yarn_app_path   + self.HADOOP_ENVS['HADOOP_CONF_DIR'] + ",\n" \
                                            + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/common/*," + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/common/lib/*,\n" \
                                            + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/hdfs/*," + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/hdfs/lib/*,\n" \
                                            + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/mapreduce/*," + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/mapreduce/lib/*,\n" \
                                            + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/yarn/*," + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/yarn/lib/*,\n"   \
                                            + self.HADOOP_ENVS['HADOOP_HOME'] + "/share/hadoop/tools/lib/*,\n"  \
                                            + self.HADOOP_ENVS['HADOOP_HOME'] + "/lib/native/\n"
        else:
            yarn_app_path = yarn_app_path   + self.HADOOP_ENVS['HADOOP_CONF_DIR'] + ",\n" \
                                            + self.HADOOP_ENVS['HADOOP_COMMON_HOME'] + "/*," + self.HADOOP_ENVS['HADOOP_COMMON_HOME'] + "/lib/*,\n" \
                                            + self.HADOOP_ENVS['HADOOP_HDFS_HOME'] + "/*," + self.HADOOP_ENVS['HADOOP_HDFS_HOME'] + "/lib/*,\n" \
                                            + self.HADOOP_ENVS['HADOOP_MAPRED_HOME'] + "/*," + self.HADOOP_ENVS['HADOOP_MAPRED_HOME'] + "/lib/*,\n" \
                                            + self.HADOOP_ENVS['YARN_HOME'] + "/*," + self.HADOOP_ENVS['YARN_HOME'] + "/lib/*\n"
        self.create_data_directories()
        self.create_log_directories()
        transforms = self.HADOOP_ENVS
        transforms.__setitem__("%HOSTNAME%", self.hostname)
        transforms.__setitem__("%YARN_APPLICATION_CLASSPATH%", yarn_app_path)
        hadoop_version = self.get_hadoop_version()
        ver = float(hadoop_version.split(' ')[1][:3])
        if ver > 2.0:
            transforms.__setitem__("%SEPARATOR%", "_")
        else:
            transforms.__setitem__("%SEPARATOR%", ".")
        self._update_hadoop_conf_template_files(transforms, template_conf_dir)
        # copy files that ends with .xml & .cfg files
        files = [ file for file in os.listdir(local_path(template_conf_dir)) \
                  if file.endswith(".xml") or file.endswith(".cfg") ]
        for config_file in files:
            cmd_str = "sudo cp -f %s %s/" %(local_path(os.path.join(template_conf_dir,config_file)), hadoop_conf_dir)
            run_shell_command(cmd_str)
            
    def install_protocol_buffer(self):
        res = {}
        run_shell_command("basename %s" %self.PROTOC_BUFFER_URL, "Get binary name of protocol buffer tar ball", res)
        binary_name = res['stdout'].split('\n')[0]
        tinctest.logger.debug("Protocol buffer binary - %s" %binary_name)
        binary_path = os.path.join(self.hadoop_install_dir, binary_name)
        tinctest.logger.debug("Protocol buffer binary Path - %s" %binary_path)
        cmd_str = "wget -O %s %s" %(binary_path, self.PROTOC_BUFFER_URL)
        res = {}
        result = run_shell_command(cmd_str, "Download binary", res)
        if not result:
            raise Exception("Failed to download protocol buffer: %s" %res['stderr'])
        res = {}
        result = run_shell_command(
                                    "tar -xzf %s -C %s" %(binary_path, self.hadoop_install_dir),
                                    "Untar binary",
                                     res
                                   )
        if not result:
            raise Exception("Failed to untar hadoop binary: %s" %res['stderr'])
        # removing the .tar.gz to get the installation directory
        protocol_buff_dir = binary_path[:-7]
        cd_protoc_buff_dir = "cd %s;" %protocol_buff_dir
        # configure
        if not run_shell_command(cd_protoc_buff_dir + "./configure","./configure protocol buff"):
            raise HadoopUtilException("Install protocol buffer : ./configure failed")
        # make
        if not run_shell_command(cd_protoc_buff_dir + "make"):
            raise HadoopUtilException("Install protocol buffer : make failed")
        # make check
        if not run_shell_command(cd_protoc_buff_dir + "make check"):
            raise HadoopUtilException("Install protocol buffer : make check failed")
        # make install
        if not run_shell_command(cd_protoc_buff_dir + "sudo make install"):
            raise HadoopUtilException("Install protocol buffer : make install failed")
    
    def build_hadoop_from_source(self):
        # if not hadoop_src_artifact_url:
            # hadoop_src_artifact_url = self.hadoop_artifact_url[:-7] + "-src.tar.gz"
        # res = {}
        # run_shell_command("basename %s" %hadoop_src_artifact_url, "To get binary name", res)
        # src_binary_name = res['stdout'].split('\n')[0]
        # tinctest.logger.debug("Hadoop Source binary - %s" %src_binary_name)
        # binary_path = os.path.join(self.hadoop_install_dir, src_binary_name)
        # tinctest.logger.debug("Hadoop source binary Path - %s" %binary_path)
        # cmd_str = "wget -O %s %s" %(binary_path, hadoop_src_artifact_url)
        # res = {}
        # result = run_shell_command(cmd_str, "Download binary", res)
        # if not result:
            # raise Exception("Failed to download hadoop binary: %s" %res['stderr'])
        # res = {}
        # result = run_shell_command(
                                    # "tar -xzf %s -C %s" %(binary_path, self.hadoop_install_dir),
                                    # "Untar binary",
                                     # res
                                   # )
        # if not result:
            # raise Exception("Failed to untar hadoop binary: %s" %res['stderr'])
        # removing the .tar.gz to get the installation directory
        # hadoop_src_install_dir = binary_path[:-7]
        # tinctest.logger.debug("Hadoop Source Installation Path - %s" %hadoop_src_install_dir)
        
        # download maven repo
        run_shell_command("sudo wget -O %s %s" %(self.MVN_REPO_LOC, self.MVN_REPO_URL), "Download the mvn repo")
        # install apache mvn
        run_shell_command("sudo yum install apache-maven -y", "Install apache maven")
        # link mvn binary
        run_shell_command("sudo ln -s /usr/share/apache-maven/bin/mvn /usr/bin/mvn")
        # install cmake
        run_shell_command("sudo yum install cmake -y", "Install cmake")
        # install google protocol buffer
        self.install_protocol_buffer()
        # build hadoop binaries
        res = {}
        cmd_str = "cd %s; mvn clean package -Pdist,native -DskipTests -Dtar" %self.hadoop_binary_loc
        if not run_shell_command(cmd_str, "Build hadoop binaries using mvn", res):
            raise HadoopUtilException("Failed to build hadoop binaries! : %s" %res['stderr'])
        res = {}
        run_shell_command("basename %s" %self.hadoop_binary_loc, "Get binary name of src dir", res)
        binary_name = res['stdout'].split('\n')[0].replace('-src','')
        self.hadoop_binary_loc = os.path.join(self.hadoop_binary_loc, "hadoop-dist/target/" + binary_name)
       
    def set_hdfs_permissions(self):
        """
        Sets the required HDFS permissions before staring up the Yarn daemons
        """
        hdfs_cmd = "sudo -u %s/bin/hdfs hdfs dfs" %self.HADOOP_ENVS['HADOOP_HOME']
        cmd_str = "%s -chmod -R 777 /" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -mkdir /tmp" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -chmod 777 /tmp" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -mkdir -p /user" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "sudo %s -mkdir -p /user/history" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "%s -chmod -R 1777 /user/history" %hdfs_cmd
        run_shell_command(cmd_str)
        cmd_str = "sudo %s -chmod -R 777 /user/" %hdfs_cmd
        run_shell_command(cmd_str)
        
    def put_file_in_hdfs(self, input_path, hdfs_path):
        if hdfs_path.rfind('/') > 0:
            hdfs_dir = hdfs_path[:hdfs_path.rfind('/')]
            cmd_str = "%s/bin/hdfs dfs -mkdir -p %s" %(self.HADOOP_ENVS['HADOOP_HOME'], hdfs_dir)
            run_shell_command(cmd_str, "Creating parent HDFS dir for path %s" %input_path)
        cmd_str = "%s/bin/hdfs dfs -put %s %s" %(self.HADOOP_ENVS['HADOOP_HOME'], input_path, hdfs_path)
        run_shell_command(cmd_str, "Copy to HDFS : file %s" %input_path)
            
    def remove_file_from_hdfs(self, hdfs_path):
        cmd_str = "%s/bin/hdfs dfs -rm -r %s" %(self.HADOOP_ENVS['HADOOP_HOME'], hdfs_path)
        run_shell_command(cmd_str, "Remove %s from HDFS" %hdfs_path)
        
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
        pass
       
