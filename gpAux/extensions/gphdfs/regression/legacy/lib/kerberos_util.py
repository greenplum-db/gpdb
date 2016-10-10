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
import sys
import re
import tinctest
import time

from rpm_util import RPMUtil
from tinctest.lib import local_path, run_shell_command
from tinctest.main import TINCException

try:
    import pexpect
except:
    raise KerberosUtilException("Couldn't import module : pexpect")
    
class KerberosUtilException(TINCException):
    """Exception class for Kerberos util"""
    pass

class KerberosUtil(object):
    """Util class for Kerberos installation"""
    
    def __init__(self, kdc_host, kdc_domain, krb_template_conf, node_list):
        self.kdc_host = kdc_host
        self.kdc_domain = kdc_domain
        self.krb_template_conf = krb_template_conf
        if node_list:
            self.list_of_hosts = node_list.append(kdc_host)
        else:
            self.list_of_hosts = [kdc_host]
        tinctest.logger.info("list_of_hosts - %s" %self.list_of_hosts)
        self.rpmutil = RPMUtil()
        self.service_cmd = "sudo /sbin/service"
        self.kadmin_cmd = "sudo /usr/sbin/kadmin.local -q "
        self.kdb5_cmd = "/usr/sbin/kdb5_util"
        self.login_user = self._get_login_user()    # get current logged-in user
        self.KRB_PKG_LIST = [
                                "krb5-server",
                                "krb5-libs",
                                "krb5-workstation"
                            ]
        self.REALM = "HD.PIVOTAL.COM"
        self.KRB_CONF = "/etc/krb5.conf"
        self.KRB_CONF_TEMPLATE = "krb5.conf.t"
        self.KDC_CONF = "/var/kerberos/krb5kdc/kdc.conf"
        self.KDC_CONF_TEMPLATE = "kdc.conf"
        self.KADMIN_ACL_CONF = "/var/kerberos/krb5kdc/kadm5.acl"
        self.KADMIN_ACL_CONF_TEMPLATE = "kadm5.acl"
        self.PRINCIPALS = [ "hdfs", "yarn", "mapred", "HTTP" ]
                                
    def _get_login_user(self):
        res = {}
        run_shell_command("whoami", "Get logged-in user", res)
        return res['stdout'].split('\n')[0]
        
    def install_kerberos(self):
        """
        Iterates through the kerberos package list
        and installs them using yum in rpm util module
        """
        for pkg in self.KRB_PKG_LIST:
            if not self.rpmutil.is_pkg_installed(pkg):
                if not self.rpmutil.install_package_using_yum(pkg, True):
                    raise KerberosUtilException("Couln't install kerberos package - %s"%pkg)
    
    def _get_domain_name(self):
        hostname = self.kdc_host
        if hostname.find('.') >= 0:
            domain = hostname[ hostname.find('.')+1 : ]
            return domain
        else:
            raise KerberosUtilException("hostname is not fully qualified domain name : %s" %hostname)
        
    def install_kerberos_conf(self):
        """
        Update the kerberos configuration files according the env
        and copy in appropriate locations
        """
        transforms = {
                        "%DOMAIN%" : self.kdc_domain,
                        "%HOSTNAME%" : self.kdc_host
                    }
        input_file_path = local_path(self.krb_template_conf + "/" + self.KRB_CONF_TEMPLATE)
        output_file_path = local_path(self.krb_template_conf + "/" + self.KRB_CONF_TEMPLATE[:-2])
        with open(input_file_path, 'r') as input:
            with open(output_file_path, 'w') as output:
                for line in input.readlines():
                    for key,value in transforms.iteritems():
                        line = re.sub(key,value,line)
                    output.write(line)
        cmd_str = "sudo cp %s %s" %(output_file_path, self.KRB_CONF)
        if not run_shell_command(cmd_str,"Copying krb5.conf"):
            raise KerberosUtilException("Couldn't copy krb5.conf")
        cmd_str = "sudo cp %s %s" %(local_path(self.krb_template_conf + "/" + self.KDC_CONF_TEMPLATE), self.KDC_CONF)
        if not run_shell_command(cmd_str,"Copying kdc.conf"):
            raise KerberosUtilException("Couldn't copy kdc.conf")
        cmd_str = "sudo cp %s %s" %(local_path(self.krb_template_conf + "/" + self.KADMIN_ACL_CONF_TEMPLATE), self.KADMIN_ACL_CONF)
        if not run_shell_command(cmd_str,"Copying kadm5.acl"):
            raise KerberosUtilException("Couldn't copy kadm5.acl")
            
    def create_krb_database(self):
        """
        Initializes kerberos database
        """
        cmd_str = "sudo %s -P changeme create -s" %self.kdb5_cmd
        if not run_shell_command(cmd_str, "Creating Kerberos database"):
            raise KerberosUtilException("Exception occured while creating Kerberos Databse!!")
            
    def start_server(self):
        """
        Starts Kerberos server
        """
        if not run_shell_command("%s krb5kdc restart" %self.service_cmd):
            raise KerberosUtilException("Couln't start kerberos service : krb5kdc\nCheck out the logs in /var/log/krb5kdc.log")
        if not run_shell_command("%s kadmin restart" %self.service_cmd):
            raise KerberosUtilException("Couln't start kerberos service : kadmin")

    def stop_server(self):
        """
        Stops kerberos server
        """
        run_shell_command("%s krb5kdc stop" %self.service_cmd)
        run_shell_command("%s kadmin stop" %self.service_cmd)
            
    def add_krb_principals(self, hosts_list):
        """
        Add principal to kerberos server
        """
        for host in hosts_list:
            for principal in self.PRINCIPALS:
                run_shell_command(self.kadmin_cmd + "\"addprinc -randkey %s/%s@%s\"" %(principal, host, self.REALM))
        # creating principal for log-in user for KDC host only
        run_shell_command(self.kadmin_cmd + "\"addprinc -randkey %s/%s@%s\"" %(self.login_user, self.kdc_host, self.REALM))
        
    def create_keytab_files(self, hosts_list):
        """
        Create all keytab files and move them to /keytab directory, since we're sudo'ing everything we'll create keytab directory right under /
        """ 
        for host in hosts_list:
            cmd_str = "ssh -o StrictHostKeyChecking=no %s \"sudo rm -rf /keytab; sudo mkdir /keytab; sudo chmod 777 /keytab\"" %host
            run_shell_command(cmd_str,"Creating keytab dir for host : %s" %host)
            # list of principals except HTTP
            for principal in self.PRINCIPALS[:-1]:
                keytab = "%s.service.keytab" %principal
                cmd_str = "%s \"xst -k %s %s/%s@%s HTTP/%s@%s\"" %(self.kadmin_cmd, keytab, principal, host, self.REALM, host, self.REALM)
                run_shell_command(cmd_str)
                # remote copy the keytab file 
                cmd_str = "sudo scp -o StrictHostKeyChecking=no %s %s:/keytab" %(keytab, host)
                run_shell_command(cmd_str, "Copy keytab file to host")
                # change ownership of the keytab file w.r.t the service
                cmd_str = "ssh -o StrictHostKeyChecking=no %s \"sudo chown %s:hadoop /keytab/%s\"" %(host, principal, keytab)
                run_shell_command(cmd_str, "Change ownership of keytab file")
            # change the access rights of the keytab files
            cmd_str = "ssh -o StrictHostKeyChecking=no %s \"sudo chmod 400 /keytab/*\"" %host
            run_shell_command(cmd_str, "Change access rights of keytab file")
            # remove the keytab files so that we won't have issues 
            # creating keytab files for other hosts
            cmd_str = "sudo rm *.keytab"
            run_shell_command(cmd_str,"Remove the keytab files")
        keytab = "%s.service.keytab" %self.login_user
        cmd_str = "%s \"xst -k %s %s/%s@%s HTTP/%s@%s\"" %(self.kadmin_cmd, keytab, self.login_user, self.kdc_host, self.REALM, self.kdc_host, self.REALM)
        run_shell_command(cmd_str, "Create keytab file for logged-in user %s" %self.login_user)
        cmd_str = "sudo chown %s:%s %s" %(self.login_user, self.login_user, keytab)
        run_shell_command(cmd_str)
        cmd_str = "cp %s /keytab" %keytab
        run_shell_command(cmd_str)
    
    def get_kerberos_ticket(self, user, host = None):
        """
        Gets kerberos ticket for the input user on host
        """
        res = {}
        run_shell_command("sudo find /usr -name kinit", "kinit command", res)
        kinit_cmd = res['stdout'].split('\n')[0]
        if not host:
            host = self.kdc_host
        if user != self.login_user:
            kinit_cmd = "sudo " + kinit_cmd
        cmd_str = "ssh -o StrictHostKeyChecking=no %s \"%s -k -t /keytab/%s.service.keytab %s/%s@%s\"" %(host, kinit_cmd, user, user, host, self.REALM)
        run_shell_command(cmd_str, "kinit for user: %s " %user)
        
    def clean(self):
        """
        Cleanup process for:
        1. Destroying previous tickets
        2. Stoping the server
        3. Destroying kerberos database
        """
        cmd_str = "kdestroy; sudo kdestroy"
        run_shell_command(cmd_str,"Destroy the tickets")
        self.stop_server()
        cmd_str = "sudo %s destroy -f" %self.kdb5_cmd
        run_shell_command(cmd_str,"Clean up Kerberos")
            
    def configure_server(self):
        """
        Init method for configuring kerberos server
        """
        self.clean()
        self.install_kerberos()
        self.install_kerberos_conf()
        self.create_krb_database()
        self.start_server()
        self.add_krb_principals(self.list_of_hosts)
        self.create_keytab_files(self.list_of_hosts)
        
if __name__  == "__main__":
    krb_util = KerberosUtil("localhost", "localdomain.com", "../configs/kerberos", ["localhost"])
    krb_util.configure_server()
    krb_util.get_kerberos_ticket("hdfs")
    krb_util.get_kerberos_ticket("gpadmin")
