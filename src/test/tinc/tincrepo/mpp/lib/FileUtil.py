#!/usr/bin/env python
# Line too long - pylint: disable=C0301
# Invalid name  - pylint: disable=C0103

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

File Utility to get content, modify content, copy, backup locally or remotely
Usage:
  from lib.util.command.FileUtil import fileUtil
"""

############################################################################
# Set up some globals, and import gptest 
#    [YOU DO NOT NEED TO CHANGE THESE]
############################################################################
import sys
import os
import shutil
import socket
import re
from tinctest.lib import local_path
from mpp.lib.String import mstring
from gppylib.commands.base import Command, REMOTE


class FileUtilError( Exception ): pass


#####
class FileUtil:
    """
    FileUtil object
    @class File
    
    @organization: DCD QA
    @exception: FileUtilError
    """

    ###
    def __init__(self):
        pass

    def _read(self, filename):
        """
        Read content of text file
        @param filename
        """
        file = open(filename, "r")
        return file.readlines()
    
    def _write(self, content, filename):
        """
        Write content to a file
        @param content: content
        @param filename: filename
        """
        filename = filename + ".tmp"
        file = open(filename, "w")
        file.writelines(content)
        file.close()
        return filename

    def copy(self, source, target, hostlist=["localhost"]):
        """
        Copy a file source to target
        @param source: source file
        @param target: target file
        @param hostlist: array of hosts, default localhost
        """
        has_remote = False
        for host in hostlist:
            if host not in (socket.gethostname(), 'localhost'):
                has_remote = True
        if has_remote:
            hosts = " -h".join(hostlist)
            cmd = "%s/bin/gpscp -h %s %s =:%s " % (GPHOME, hosts, source, target)
            cmd = Command(name='copy a file from source to target ', cmdStr = cmd).run(validateAfter=True)
        else:
            shutil.copy(source, target)            

    def backup(self, filename, hostname="localhost", backupext=".bak"):
        """
        Creates a backup file
        @param filename: filename
        @param hostname: hostname, default localhost
        @param backupext: extension of the backupfile
        """
        backupfile = filename+backupext
        self.copy(filename, backupfile, [hostname])

    
    def delete_content(self, filename, deleteMe, hostname="localhost"):
	"""	
	@param filename: filename
	@param deleteMe: the line which which is to be deleted fro the file in regular expression
	@param hostname: hostname, default localhost
	"""
	content = self.get_content(filename, hostname)
	i=0;
	while (i<len(content)):
		if re.search(deleteMe,content[i]) is not None:
			del content[i]
		else:
		 	i=i+1
	tmpfile = self._write(content, filename)
        self.copy(tmpfile, filename, [hostname])

    def search_content(self, searchKey, filename, hostname="localhost"):
        """
        Search content of a file
        @param searchKey: search key in regular expression
        @param filename: filename
        @param hostname: hostname, default localhost
        @return True or False
        """
        content = self.get_content(filename, hostname)
        return mstring.match_array(content, searchKey)
    	
  

    def append_content(self, entry, filename, hostname="localhost"):
        """
        Search content of a file
        @param entry: text entry
        @param filename: filename
        @param hostname: hostname, default localhost
        @return True or False
        """
        if hostname in (socket.gethostname(),'localhost'):
            cmdLine = "echo %s >> %s" % (entry, filename)
            cmd = Command(name=' Search content of a file', cmdStr = cmdLine)
        else:
            cmdLine = '"echo %s >> %s"' % (entry, filename)
            cmd = Command(name=' Search content of a file', cmdStr = cmdLine, ctxt=REMOTE, remoteHost=hostname)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        (ok, out) = (result.rc, result.stdout)
        if not ok:
            return out
        else:
            raise FileUtilError("Error adding content for %s" % filename)
        
    def get_content(self, filename, hostname="localhost"):
        """
        Get the content of a text file
        @param filename: Absolute location of the file
        @param hostname: hostname
        """
        if hostname in (socket.gethostname(),'localhost'):
            cmdLine = "cat %s" % filename
            cmd = Command(name=' Search content of a file', cmdStr = cmdLine)
        else:
            cmdLine = '"cat %s"' % filename
            cmd = Command(name=' Search content of a file', cmdStr = cmdLine, ctxt=REMOTE, remoteHost=hostname)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        (ok, out) = (result.rc, result.stdout)
        if not ok:
            return out
        else:
            raise FileUtilError("Error getting content for %s" % filename)

    def modify(self, filename, search, replace, hostname="localhost"):
        """
        Modify the content of a text file. We assume that we are modifying the
        content on the same machine. Else, specify the host
        @param filename: Absolute location of the file
        @param search: search string
        @param replace: replace string
        @param hostname: hostname
        """
        self.backup(filename, hostname)
        content = self.get_content(filename, hostname)
        replacedContent = mstring.replace_array(content, search, replace)
        tmpfile = self._write(replacedContent, filename)
        self.copy(tmpfile, filename, [hostname])
        
    def modify_regexp(self, filename, regex_pattern, replace, hostname="localhost" ): 
        """
        
        Modify the content of a text file. We assume that we are modifying the
        content on the same machine. Else, specify the host
        @param filename: Absolute location of the file
        @param regex_pattern: search pattern in regular expresssion
        @param replace: replace string
        @param hostname: hostname
        """
        self.backup(filename, hostname)
        content = self.get_content(filename, hostname)
        replacedContent = mstring.replace_content_regex(content, regex_pattern, replace)
        tmpfile = self._write(replacedContent, filename)
        self.copy(tmpfile, filename, [hostname])   
        
    def exists(self, filename, hostname="localhost"):
        cmdLine = "ls -d %s" % filename
        cmd = Command(name=' Search content of a file', cmdStr = cmdLine)
        cmd.run(validateAfter=False)
        result = cmd.get_results()
        (ok, out) = (result.rc, result.stdout)
        """
        @note: Check content for "No such file or directory"
        [jsoedomo-mbp] /Users/jsoedomo/Greenplum/cdbfast/private/jsoedomo/storage/gp_filespace_tablespace/gpfs_a_01/primary/gp0/pgsql_tmp
        jsoedomo-mbp:functional jsoedomo$ python /Users/jsoedomo/greenplum-db-devel/bin/gpssh -h jsoedomo-mbp -u jsoedomo ls -d /Users/jsoedomo/Greenplum/cdbfast/private/jsoedomo/storage/gp_filespace_tablespace/gpfs_a_01/primary/gp0/pgsql_tmp/pgsql_tmp_*
        [jsoedomo-mbp] ls: /Users/jsoedomo/Greenplum/cdbfast/private/jsoedomo/storage/gp_filespace_tablespace/gpfs_a_01/primary/gp0/pgsql_tmp/pgsql_tmp_*: No such file or directory
        """
        pattern = [".*No such file or directory.*"]
        return not mstring.match_array(out, pattern) # If pattern is not match, then file exist
    
fileUtil = FileUtil()


##########################
if __name__ == '__main__':
    fileUtil = FileUtil()
    out = fileUtil.get_content(local_path("FileUtil.py"))
