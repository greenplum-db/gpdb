"""
Copyright (c) 2004-Present Pivotal Software, Inc.

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
import re
import unittest

import ConfigParser
import json
import httplib
from xml.etree import ElementTree as ET
import urllib
import urllib2

from time import sleep

from xml.etree import ElementTree as ET
from gppylib.commands.base import Command, REMOTE
from tinctest.lib import local_path
from tinctest.lib.system import TINCSystem

class WS():
                def parse_xml(self,xml_input_file):	
                        """Function parse_xml takes a xml as input and returns the tags and values"""
                        tinctest.logger.info('In function parse_xml')
                        xml_input = ET.parse(xml_input_file)
                        return xml_input
                
                def generate_config_file(self,greenplum_path_file,pgport,cc_install_dir,port,mdd):
                        configFile_template = local_path("../gpdb/tests/utilities/commandcenter/configs/config_template.txt")
                        configFile = local_path("../gpdb/tests/utilities/commandcenter/configs/config.txt")
                        cc_source = cc_install_dir + "/gpcc_path.sh"
                        transforms = {'%pgport%': pgport,
                              '%source%': greenplum_path_file,
                              '%instance_name%': cc_install_dir,
                              '%port%': port,
                              '%mdd%': mdd,
                              '%cc_source%': cc_source 
                              }
                        cmd_str = 'chmod 777 %s; chmod 777 %s;'% (configFile_template ,configFile)
                        tinctest.logger.info(cmd_str)
                        cmd = Command (name = 'chmod',
                                   cmdStr = cmd_str,
                                   ctxt = REMOTE, remoteHost = 'localhost')

                        cmd.run()                        
			                                 
                        TINCSystem.substitute_strings_in_file(configFile_template,
                                                          configFile,
                                                          transforms)	

                def read_config(self,param):
                        """Function to read configuration file and get parameters needed for test"""
                        configParser = ConfigParser.RawConfigParser()
                        configFilePath = local_path("../gpdb/tests/utilities/commandcenter/configs/config.txt")
                        configParser.read(configFilePath)
                        if (param == 'metadata'):
                                tinctest.logger.info('Fetching metdata')
                                instance_name = configParser.get('valid','instance_name')
                                machine = configParser.get('metadata','machine')
                                pgport = configParser.get('metadata','pgport')
                                port = configParser.get('metadata','port')
                                mdd = configParser.get('metadata','mdd')
                                source = configParser.get('metadata','source')
                                cc_source = configParser.get('metadata','cc_source')
                                return(machine,pgport,port,mdd,source,instance_name,cc_source)
                        else:
                                tinctest.logger.info('Fetching username and password')
                                username = configParser.get(param,'username')
                                password = configParser.get(param,'password')
                                return (username, password)

                def get_sessionid(self,status_file):
                        """ Function get_status checks is login was successful and if yes, returns the valid session id else returns 0 """
                        xml_input = self.parse_xml(status_file)
                        base = xml_input.getroot()
                        tinctest.logger.info('In function get_sessionid')
                        if xml_input.findtext('status'):
                                tinctest.logger.info('found status')
                                status = xml_input.find('status').text
                                tinctest.logger.info(status)
                        else:
                                tinctest.logger.info('status tag not found..failure detected')
                                status = 'FAIL'
                        if (status == 'SUCCESS'):
                                session_id = xml_input.find('sessionid').text
                        else:
                                tinctest.logger.info('Login unsuccessful')
                                session_id = 0
                        tinctest.logger.info('sessionid: ')
                        tinctest.logger.info(session_id)
                        return session_id

                def restart_db(self,restart_param):
                        """ Function to do various db operations(stop, start,restart,start in master/restricted mode) based on passed parameter """
                        (machine,pgport,port,mdd,source,instance_name,cc_source) = self.read_config('metadata')
                        if (restart_param != 'Stop' and restart_param != 'Start'):
                                if (restart_param == 'None'):
                                        stop_param = '-air'
                                else:
                                        stop_param = '-ai'
                                tinctest.logger.info('Stopping db or restarting db based on restart_param passed')
                                cmd_str = 'export MASTER_DATA_DIRECTORY=%s; \
                                           export PGPORT=%s; \
                                           source %s; \
                                           gpstop %s;' \
                                           % (mdd , pgport, source,stop_param)

                                cmd = Command (name ='stopdb',
                                           cmdStr = cmd_str,
                                           ctxt = REMOTE, remoteHost = 'localhost')
                                cmd.run()
                                if (restart_param == 'masteronly'):
                                        tinctest.logger.info('Starting db in master only mode')
                                        cmd_str = 'export MASTER_DATA_DIRECTORY=%s; \
                                                   export PGPORT=%s; \
                                                   source %s; \
                                                   export GPSTART_INTERNAL_MASTER_ONLY=1; \
                                                   gpstart -a -m;' \
                                                   % (mdd , pgport, source)
                                        tinctest.logger.info(cmd_str)

                                        cmd = Command (name ='startmasteronly',
                                                   cmdStr = cmd_str,
                                                   ctxt = REMOTE, remoteHost = 'localhost')
                                        tinctest.logger.info(cmd)
                                        cmd.run()
                                elif (restart_param == 'restricted'):
                                        tinctest.logger.info('Starting db in restricted mode')
                                        cmd_str = 'export MASTER_DATA_DIRECTORY=%s; \
                                                   export PGPORT=%s; \
                                                   source %s; \
                                                   gpstart -R -a;' \
                                                   % (mdd , pgport, source)
                                        tinctest.logger.info(cmd_str)
                                        cmd = Command (name ='startrestricted',
                                                   cmdStr = cmd_str,
                                                   ctxt = REMOTE, remoteHost = 'localhost')
                                        tinctest.logger.info(cmd)
                                        cmd.run()
                                elif (restart_param == 'None'):
                                        tinctest.logger.info('restart_param is none')
                        elif (restart_param == 'Stop'):
                                tinctest.logger.info('Stopping db')
                                stop_param = '-ai'
                                cmd_str = 'export MASTER_DATA_DIRECTORY=%s; \
                                           export PGPORT=%s; \
                                           source %s; \
                                           gpstop %s;' \
                                           % (mdd , pgport, source,stop_param)

                                cmd = Command (name ='stopdb',
                                           cmdStr = cmd_str,
                                           ctxt = REMOTE, remoteHost = 'localhost')
                                cmd.run()
                        elif (restart_param == 'Start'):
                                tinctest.logger.info('Starting db')
                                stop_param = '-a'
                                cmd_str = 'export MASTER_DATA_DIRECTORY=%s; \
                                           export PGPORT=%s; \
                                           source %s; \
                                           gpstart %s;' \
                                           % (mdd , pgport, source,stop_param)

                                cmd = Command (name ='startdb',
                                           cmdStr = cmd_str,
                                           ctxt = REMOTE, remoteHost = 'localhost')
                                cmd.run()

            # Function login to login to the CC to execute other tests
                def login(self,param):
                        """ Function to login to the CC to execute other tests """
                        tinctest.logger.info("-------------------------------")
                        tinctest.logger.info('Login operation')
                        tinctest.logger.info("-------------------------------")
                
                        (machine,pgport,port,mdd,source,instance_name,cc_source) = self.read_config('metadata')	
                        host = machine+":"+port
                        h = httplib.HTTPConnection(host)
                        url="http://"+machine+":"+port+"/"

                        tinctest.logger.info('Fetching username and password from config file')

                        (username, password) = self.read_config(param)

                        tinctest.logger.info('In function login')
                        tinctest.logger.debug('Establising connection')
                        data = {'username':username,'password':password}
                        encoded_data = urllib.urlencode(data)

                        tinctest.logger.debug('Opening url'+url)
                        request = urllib2.Request(url+"logon", None)
                        rs = urllib2.urlopen(request, encoded_data)
                        h.request('POST',"/logon", encoded_data)

                        login_msg = rs.read()
                        # write the xml returned to a file for further processing
                        login_status_xml = local_path("../gpdb/tests/utilities/commandcenter/data/login_status")
                        f = open(login_status_xml, 'w')
                        f.write(login_msg)
                        f.close()

                        # Call the get_sessionid function to get the session_id
                        session_id = self.get_sessionid(login_status_xml)
                        return session_id
                
                def get_web_results(self,machine,port,instance_name,session_id,service):
                        """ Function to form url and fetch web service results """
                        tinctest.logger.info("-------------------------------")
                        tinctest.logger.info('Constructing url and getting web service results')
                        tinctest.logger.info("-------------------------------")
                        url="http://"+machine+":"+port+"/"
                        cookie = "gpperfmon_instance_"+instance_name+"="+session_id
                        headers = {"Cookie": cookie,"Accept" : "application/json"}
                        #data = {'gpperfmon_instance_cc':session_id}
                        if (session_id != "0"):
                                request = urllib2.Request(url+service, None,headers)
                        else:
                                request = urllib2.Request(url+service, None)
                        rs = urllib2.urlopen(request)
                        ws_msg = rs.read()
                        return ws_msg


