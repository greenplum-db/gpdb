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
import tinctest
import socket
import re

from tinctest.lib.gpfdist import gpfdist, GpfdistError
from tinctest.lib import local_path, run_shell_command
from tinctest.main import TINCException
from lib.phd_rpm_util import PHDRpmUtil
from lib.cdh_rpm_util import CDHRpmUtil
from lib.apache_tar_util import ApacheTarUtil
from lib.kerberos_util import KerberosUtil

class HadoopIntegrationException(TINCException):
    """Exception class for hadoop integration test."""
    pass

class HadoopIntegration(object):           
    """Integrates Hadoop and GPDB."""

    def __init__(self, hadoop_type, gphdfs_connector, hadoop_artifact_url, hadoop_install_dir, hadoop_data_dir, template_conf_dir, secure_hadoop, node_list):
        self.hadoop_type = hadoop_type
        self.gphdfs_connector = gphdfs_connector
        self.hadoop_artifact_url = hadoop_artifact_url
        self.hadoop_install_dir = hadoop_install_dir
        self.hadoop_data_dir = hadoop_data_dir
        self.template_conf_dir = template_conf_dir
        
        self.secure_hadoop = secure_hadoop
        self.node_list = node_list
        self.cur_dir = os.path.abspath(os.path.dirname(__file__))
        self.gpfdistport = '8080'
        (host, domain) = self._get_host_and_domain()
        if host:
            self.hostname = host
        else:
            self.hostname = 'localhost'
        if domain:
            self.domain = domain
        else:
            self.domain = 'localdomain.com'
        self.gpfdistport = self._get_gpfdistport(self.gpfdistport)
    
    def _get_gpfdistport(self,gpfdistport):
        result = 0
        while result == 0:
            cmd_str = "netstat -a| grep %s" %gpfdistport
            res = {'rc':0, 'stderr':'', 'stdout':''}
            run_shell_command(cmd_str,'Grep Netstat',res)
            result = res['rc']
            if result == 0:
                gpfdistport = str(int(gpfdistport) + 1)
            else:
                return gpfdistport
    
    def _get_host_and_domain(self):
        hostname = ''
        domain = ''
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command('hostname','Get hostname command',res)
        result = res['stdout']
        if len(result) > 0:
            hostname = result.split('\n')[0]
            if hostname.find('.') >= 0:
                domain = hostname[ hostname.find('.')+1 : ]
                hostname = hostname[ : hostname.find('.') ]
        return (hostname, domain)

    def _create_test_jars(self, export_env, java_classpath):
        cmd_str = "%s cd %s; javac -cp %s javaclasses/*.java" %(export_env, self.cur_dir, java_classpath)
        if not run_shell_command(cmd_str, "Compiling java classes"):
            raise HadoopIntegrationException("Error while compiling java classes!")
        cmd_str = "cd %s; jar cf maptest.jar javaclasses/*.class" %self.cur_dir
        if not run_shell_command(cmd_str, "Creating jar file"):
            raise HadoopIntegrationException("Error while creating the jar!")
            
    def _create_java_cmd_string(self, export_env, java_classpath):
        envvar = '-Dhdfshost=' + self.fqdn + ' -Ddatanodeport=8020 -Djobtrackerhost=' + self.fqdn + ' -Djobtrackerport=8020 ' 
        java_cmd = "%s java -cp %s:%s/maptest.jar %s" % (export_env, java_classpath, self.cur_dir, envvar)
        return java_cmd

    def _create_test_data(self, datasize, large_datasize, test_data_types):
        """
        Creates the test data required for the sqls to run.
        """
        data_dir = self.cur_dir + '/tmp/text'
        run_shell_command('mkdir -p %s' %data_dir)
        for data_type in test_data_types:
            data_type_file = data_dir + "/" + data_type + ".txt"
            cmd_str = "python %s/lib/create_data.py %s %s > %s" %(self.cur_dir, datasize, data_type, data_type_file)
            run_shell_command(cmd_str, "Create data for type -> %s" %data_type)
        cmd_str = "python %s/lib/create_data.py %s regression > %s/tmp/random_with_seed_1.largetxt" %(self.cur_dir, large_datasize, self.cur_dir)
        run_shell_command(cmd_str, "Create regress test data")
        cmd_str = "python %s/lib/create_data.py %s all > %s/all_20.txt" %(self.cur_dir, str(int(datasize) * 20), data_dir)
        run_shell_command(cmd_str, "Create regress test data for datasize * 20")
        cmd_str = "python %s/lib/create_data.py %s all > %s/all_100.txt" %(self.cur_dir, str(int(datasize) * 100), data_dir)
        run_shell_command(cmd_str, "Create regress test data for datasize * 100")
        # create test data for typemismatch test
        run_shell_command("sed 's/bigint/text/g' %s/bigint.txt > %s/bigint_text.txt" %(data_dir, data_dir), "create test data for typemismatch test")
        # copy composite file into data_dir
        run_shell_command("cp %s/sql/data/compositeType.txt %s" %(self.cur_dir, data_dir), "Copy composite file" )
        # put test data files in HDFS
        self.hadoop_util.put_file_in_hdfs("%s/sql/regression/data/*" %self.cur_dir, "/plaintext/") 
        self.hadoop_util.put_file_in_hdfs("%s/tmp/random_with_seed_1.largetxt" %self.cur_dir, "/plaintext/random_with_seed_1.largetxt") 
        self.hadoop_util.put_file_in_hdfs("%s/tmp/text/all_100.txt" %self.cur_dir, "/plaintext/all_100.txt")
        self.hadoop_util.put_file_in_hdfs("%s/tmp/text/all.txt" %self.cur_dir, "/plaintext/all.txt") 
        self.hadoop_util.put_file_in_hdfs("%s/tmp/text/timestamp.txt" %self.cur_dir, "/plaintext/timestamp.txt") 
        self.hadoop_util.put_file_in_hdfs("%s/tmp/text/varchar.txt" %self.cur_dir, "/plaintext/varchar.txt") 
        self.hadoop_util.put_file_in_hdfs("%s/sql/data/*" %self.cur_dir, "/plaintext/")
        
        # start gpfdist process
        # gpfdist_process = gpfdist(self.gpfdistport, self.fqdn)
        # assert (gpfdist_process.start(options=' -d %s' %data_dir))
        self.start_gpfdist_process(data_dir)
        
    def start_gpfdist_process(self, data_dir):
        # start gpfdist process:
        # we have seen cases where the gfdist process don't start on particular port,
        # due to connection bind error or due to "FATAL cannot create socket on port 8080" 
        # this occurs in spite of checking netstat for used ports at the beginning
        # so as a hack, we keep on trying different ports until gpfdist is started
        gpfdist_process_started = False
        while not gpfdist_process_started:
            gpfdist_process = gpfdist(self.gpfdistport, self.fqdn)
            try:
                gpfdist_process.start(options=' -d %s' %data_dir)
            except GpfdistError as message:
                tinctest.logger.warn("Couldn't setup gpfdist on port %s"%self.gpfdistport)
                gpfdist_process_started = False
                self.gpfdistport = str(int(self.gpfdistport) + 1)
            else:
                gpfdist_process_started = True
                tinctest.logger.info("Started gpfdist on port %s"%self.gpfdistport)
    
    def get_ip_address(self):
        return socket.gethostbyname(socket.gethostname())
        
    def _setup_gpdb_configurations(self, gphome, mdd, gpdb_template_conf, hadoop_home, hadoop_common_home, hadoop_guc):
        """
        Updates the gpdb template confgiration files per the current env.
        Also copies required configuration files.
        """
        text = "\n### Hadoop specific variables\n"
        if self.secure_hadoop:
            text = text + "export HADOOP_SECURE_DN_USER=hdfs\n"
        text =  text +  "export CLASSPATH=$HADOOP_HOME/lib\n"   \
                        "export GP_JAVA_OPT=\"$GP_JAVA_OPT -Djava.library.path=$HADOOP_HOME/lib/native/\"\n"  \
                        "export GP_HADOOP_CONN_JARDIR=lib/hadoop\n"    \
                        "export GP_HADOOP_CONN_VERSION=%s\n" %self.gphdfs_connector
               
        greenplum_path_file = os.path.join(gphome,"greenplum_path.sh")
        self.hadoop_util.append_text_to_file(greenplum_path_file, text)
        host = str(self.get_ip_address()) + "/32"
        text =  "local    all     _hadoop_perm_test_role    trust\n"    \
                "host    all     _hadoop_perm_test_role  %s  trust\n" %host
        self.hadoop_util.append_text_to_file(mdd + "/pg_hba.conf", text)
        cmd_str = "source %s; gpconfig -c gp_hadoop_target_version -v %s" %(greenplum_path_file, hadoop_guc)
        run_shell_command(cmd_str, "Setting gp_hadoop_target_version as %s" %hadoop_guc)
        cmd_str = "source %s; gpstop -air" %greenplum_path_file
        assert run_shell_command(cmd_str, "Restart GPDB")
        # create hadoop_env.sh file based on the hadoop type
        transforms = {'%CONNECTOR%' : self.gphdfs_connector, '%JAVA_HOME%' : self.hadoop_util.get_java_home()}
        input_file_path = local_path(gpdb_template_conf + "/hadoop_env.sh.%s.t" %self.hadoop_type)
        output_file_path = local_path(gpdb_template_conf + "/hadoop_env.sh")
        with open(input_file_path, 'r') as input:
            with open(output_file_path, 'w') as output:
                for line in input.readlines():
                    for key,value in transforms.iteritems():
                        line = re.sub(key,value,line)
                    output.write(line)
        cmd_str = "cp %s/hadoop_env.sh %s/lib/hadoop/hadoop_env.sh" %(gpdb_template_conf, gphome)
        run_shell_command(cmd_str)
        cmd_str = "sudo cp %s/lib/hadoop/%s.jar %s" %(gphome,self.gphdfs_connector, hadoop_common_home)
        run_shell_command(cmd_str, "Copying the gphds connector")
    
    def _validate_hostname(self):
        etc_hosts_file = "/etc/hosts"
        cmd_str = "sudo egrep \"%s\" %s" %(self.fqdn, etc_hosts_file)
        # check if hostname present or not, add if not present
        if not run_shell_command(cmd_str, "Checking hostname - %s in /etc/hosts" %self.fqdn):
            ip_addr = self.get_ip_address()
            text_to_append = ip_addr + "    " + self.fqdn
            # give write permissions to etc/hosts file
            run_shell_command("sudo chmod o+w %s" %etc_hosts_file)
            with open(etc_hosts_file, "a") as append_file:
                append_file.write(text_to_append)
            # remove write permissions from etc/hosts file
            run_shell_command("sudo chmod o-w %s" %etc_hosts_file)
        
    
    def integrate(self):
        """
        Integrates Hadoop and GPDB by performing the following:
        
        1. Setup kerberos server
        2. Setup hadoop cluster
        3. Setup GPDB configurations
        4. Create sql sepcific test data
        """
        # check for GPHOME and MASTER_DATA_DIRECTORY
        # throw exception is not set
        gphome = os.getenv("GPHOME")
        if not gphome:
            raise HadoopIntegrationException("GPHOME not set!!")
        mdd = os.getenv("MASTER_DATA_DIRECTORY")
        if not mdd:
            raise HadoopIntegrationException("MASTER_DATA_DIRECTORY not set!!")
        self.fqdn = self.hostname + '.' + self.domain
        # check if hostname is present in /etc/hosts
        # if not append the hostname to file
        self._validate_hostname()
        # setup kerberos server if security is enabled
        if self.secure_hadoop:
            self.kerberos_template_conf = local_path(os.path.join(self.template_conf_dir, "kerberos"))
            self.kerberos_util = KerberosUtil(self.fqdn, self.domain, self.kerberos_template_conf, self.node_list)
            self.kerberos_util.configure_server()
            self.kerberos_util.get_kerberos_ticket("hdfs")
            self.kerberos_util.get_kerberos_ticket("gpadmin")
        # setup hadoop cluster
        hadoop_conf_dir = local_path(os.path.join(self.template_conf_dir, "hdfs/rpm"))
        if self.hadoop_type == "phd":
            self.hadoop_util = PHDRpmUtil(self.hadoop_artifact_url, self.hadoop_install_dir, self.hadoop_data_dir, hadoop_conf_dir, self.fqdn, self.secure_hadoop)
            hadoop_guc = "gphd-2.0"
        elif self.hadoop_type == "cdh":
            self.hadoop_util = CDHRpmUtil(self.hadoop_artifact_url, self.hadoop_install_dir, self.hadoop_data_dir, hadoop_conf_dir, self.fqdn, self.secure_hadoop)
            hadoop_guc = "cdh4.1"
        elif self.hadoop_type == "apache":
            self.hadoop_util = ApacheTarUtil(self.hadoop_artifact_url, self.hadoop_install_dir, self.hadoop_data_dir, hadoop_conf_dir, self.fqdn, self.secure_hadoop)
            hadoop_guc = "gphd-2.0"
        # setup up hadoop cluster
        self.hadoop_util.init_cluster()
        hadoop_home = self.hadoop_util.get_hadoop_env()['HADOOP_HOME']
        hadoop_common_home = self.hadoop_util.get_hadoop_env()['HADOOP_COMMON_HOME']
        if self.hadoop_type == "apache":
            hadoop_common_home = hadoop_common_home + "common"
        # setup up GPDB configurations & test data
        gpdb_template_conf = local_path(os.path.join(self.template_conf_dir, "gpdb"))
        self._setup_gpdb_configurations(gphome, mdd, gpdb_template_conf, hadoop_home, hadoop_common_home, hadoop_guc)
        export_env = "export HADOOP_HOME=%s; source %s/lib/hadoop/hadoop_env.sh;" %(hadoop_home, gphome)
        java_classpath = ".:$CLASSPATH:%s/lib/hadoop/%s" %(gphome, self.gphdfs_connector)
        self._create_test_jars(export_env, java_classpath)
        self.java_cmd = self._create_java_cmd_string(export_env, java_classpath)
        test_data_types = [
                            'regression', 'time', 'timestamp', 'date',  \
                            'bigint', 'int', 'smallint', 'real', 'float',   \
                            'boolean', 'varchar', 'bpchar', 'numeric', 'text', 'all'
                          ]
        datasize = 5000
        largedatasize = str(int(datasize) * 2000)
        self._create_test_data(datasize, largedatasize, test_data_types)
        
    def get_substitutions(self):
        """
        For each sql test, this method will be called
        by the SQLTestCase implementing class.
        Used for making the substitutions in the sql just before its run
        """
        hadoop_home = self.hadoop_util.get_hadoop_env()['HADOOP_HOME']
        substitutions = {'%gpfdistPort%': self.gpfdistport,
                         '%localhost%': self.fqdn,
                         '%cmdstr%': self.java_cmd,
                         '%HADOOP_HOST%': self.fqdn + ":8020",
                         '%HDFSaddr%': self.fqdn + ":8020",
                         '%MYD%': os.path.join(self.cur_dir, "sql"),
                         '%HADOOP_FS%': hadoop_home,
                         '%HADOOP_HOME%': hadoop_home
                         }
        return substitutions

    def teardown(self):
        """
        This method gets called after each sql test case completes.
        Purpose is to clean up the HDFS after a sql test run        
        """
        tinctest.logger.debug("Running teardown method")
        self.hadoop_util.remove_file_from_hdfs('/extwrite/')
        self.hadoop_util.remove_file_from_hdfs('/mapreduce/')
        self.hadoop_util.remove_file_from_hdfs('/mapred/')

    def teardownclass(self):
        """
        This method will be called after Each test-suite is finished executing
        """
        # clean up hadoop
        self.hadoop_util.cleanup()
        # clean up kerberos
        self.kerberos_util.clean()
