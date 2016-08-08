#!/usr/bin/env python
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

import os, sys, unittest, string, platform, shutil
from tinctest.lib import local_path, run_shell_command, Gpdiff
from mpp.lib.PSQL import PSQL

# ============================================================================
# gpmapreduce [options] -f file.yml [dbname [username]]

def ansFile(fname ):
    # Uses Jeff's new gpdiff.pl/atmsort.pl compatible with pyodbc
    # For pyODB, we have to use a different expected result
    # Check for platform dependent expecte results
    curplatform = sys.platform
    ext = ".ans"
    if string.find( curplatform, 'sun' ) >= 0:
        ext = ".ans.sun"
    elif string.find( curplatform, 'darwin' ) >= 0:
        ext = ".ans.osx"
    # RH64 and SUSE return "linux2" for sys.platform
    # Will nee to use /etc/SuSE-release or /etc/redhat-release
    # Note: http://docs.python.org/library/sys.html
    # Linux (2.x and 3.x), platform value = linux2
    elif curplatform.startswith("linux"):
        if os.path.exists("/etc/SuSE-release"):
            ext = ".ans.suse"
        elif os.path.exists("/etc/redhat-release"):
            if platform.machine() == "i686":
                ext = ".ans.linux-i686"
            elif platform.machine() == "x86_64":
                ext = ".ans.linux-x86_64"

    if not os.path.exists( os.path.splitext( fname )[0] + ext ):
        ext = ".ans"
    return os.path.splitext( fname )[0] + ext

def changeExtFile( fname, ext = ".diff", outputPath = "" ):
    if len( outputPath ) == 0:
        return os.path.splitext( fname )[0] + ext
    else:
        filename = fname.split( "/" )
        fname = os.path.splitext( filename[len( filename ) - 1] )[0]
        return outputPath + "/" + fname + ext

class MAPREDUCE(unittest.TestCase):
    cmd = 'gpmapreduce'
    moduleName = 'mapreduce'
    fixup = 'bin/fixup.pl'

    def __init__(self):
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command ('which gpmapreduce', 'Check whether gpmapreduce is found', res)
        if res['rc'] != 0:
            ignalys.exit('Unable to run '+MAPREDUCE.cmd)

        # Drop and Create plperlu, plperl, plpython, plpythonu languages
        languages = ['plperl','plperlu','plpython','plpythonu']
        for(fileNo,lang) in enumerate(languages):
            PSQL.run_sql_command(sql_cmd = 'drop language '+lang)
            PSQL.run_sql_command(sql_cmd = 'create language '+lang)

    def outFile(self, fname, outputPath = "" ):
        return changeExtFile( fname, ".out", outputPath )

    def run(self,ifile=None,ofile=None,dbname=None, username=None):
        if dbname == None:
            dbname = os.environ.get('PGDATABASE')
        if username == None:
            if not os.environ.get('PGUSER'):
                username = ''
            else:
                username = os.environ.get('PGUSER')
        res = {'rc':0, 'stderr':'', 'stdout':''}
        run_shell_command (MAPREDUCE.cmd + ' -f %s %s %s > %s 2>&1 ' % (ifile, dbname, username, ofile), 'run gpmapreduce command', res)
        return res['rc']


    def runYml(self, ifile, db=os.environ.get('PGDATABASE'), uname = None):
        return self.run(ifile = ifile, ofile = self.outFile(ifile), dbname=db, username=uname)

    def replaceTemplate(self, file, path):
        # Uses Jeff/Caleb fixup.pl to replace abs_srcdir and hostname
        fixup_file = os.path.join(path, MAPREDUCE.fixup)
        if not os.path.exists(fixup_file):
            return
        else:
            res = {'rc':0, 'stderr':'', 'stdout':''}
            run_shell_command(fixup_file+" "+file, 'run fixup command', res)
            return res
    

    def appendMatchSub(self,ifile):
        msstr  = "-- start_matchsubs\n"
        msstr += "-- m/mapreduce_\d+_/\n"
        msstr += "-- s/mapreduce_\d+/mapreduce_DUMMY/\n"
        msstr += "-- m/\(\w+.\w+:[0-9]+\)$/\n"
        msstr += "-- s/\(\w+.\w+:[0-9]+\)$/\(file:line\)/\n"
        msstr += "-- end_matchsubs\n"

        str = ""
        ignore = False
        if os.path.isfile(ifile):
            f = open(ifile)
            for s in f.xreadlines():
                # Start ignoring the block and replace with the new match sub
                if s.find("-- start_matchsubs")==0:
                    ignore = True
                elif s.find("-- end_matchsubs")==0:
                    ignore = False
                    s = "";
                if not ignore:
                    str += s;
            f.close()
            # Overwrite to the same file
            tmp_file = ifile + '.tmp'
            f1 = open(tmp_file, 'w')
            f1.write(msstr + str)
            f1.close()
            shutil.move(tmp_file, ifile)


    def checkResult(self, ifile, optionalFlags="", ignoreInfoLines = False):

        """
        PURPOSE: compare the actual and expected output files and report an 
            error if they don't match.
        PARAMETERS:
            ifile: the name of the .sql file whose actual and expected outputs 
                we want to compare.  You may include the path as well as the 
                filename.  This function will process this file name to 
                figure out the proper names of the .out and .ans files.
            optionalFlags: command-line options (if any) for diff.  
                For example, pass " -B " (with the blank spaces) to ignore 
                blank lines.
            ignoreInfoLines: set to True to ignore lines from gpcheckcat (and 
                possibly other sources) that contain "[INFO]" so we get diffs 
                only for errors, warnings, etc.
        LAST MODIFIED: 
            2010-05-06 mgilkey
                I added the ignoreInfoLines option so that we can ignore 
                differences in gpcheckcat output that are not significant.
            2010-02-17 mgilkey
                I added the "optionalFlags" parameter.
        """

        f1 = self.outFile(ifile)
        f2 = ansFile(ifile)
        self.appendMatchSub(f1)
        self.appendMatchSub(f2)
        Gpdiff.are_files_equal(out_file = f1, ans_file = f2)
        return True

mapreduce = MAPREDUCE()

