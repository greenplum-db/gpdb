#!/usr/bin/env python

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

gp_procedural_languages class is used to
    1) check if procedurel languages are supported by gpdb
    2) check if procedural languages are installed on gpdb
    3) install procedural languages on gpdb
    4) drop procedural languages from gpdb
"""

############################################################################
# Set up some globals, and import gptest
#    [YOU DO NOT NEED TO CHANGE THESE]
############################################################################
import sys, os
import tinctest
from mpp.lib.PSQL import PSQL

class gp_procedural_languages:
    """
    gp_procedural_languages class is used to
        1) check if procedural languages are supported by gpdb
        2) check if procedural languages are installed on gpdb
        3) install procedural languages on gpdb
        4) drop procedural languages from gpdb
    @class gp_procedural_languages
    
    @organization: DCD QA
    @contact: Ruilong Huo
    """


    def __init__(self):
        """ 
        Constructor for gp_procedural_languages
        """
        pass

    def isSupported(self, plname=None):
        """
        check if a given procedural language is supported by gpdb
        @param plname: the name of the procedural language to be checked
        @return: True if the given procedural language is supported by gpdb, False if it is not supported
        """
        if plname is None:
            plname = ""
        else:
            plname = plname.lower()

        sql = "SELECT COUNT(*) FROM (SELECT tmplname AS lanname FROM pg_pltemplate UNION SELECT lanname AS lanname FROM pg_language) t WHERE lanname='%s';" % (plname)

        cmd = PSQL(sql_cmd = sql, flags = '-q -t', dbname=os.environ.get('PGDATABASE'))
        tinctest.logger.info("Running command - %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        ok = result.rc
        out = result.stdout.strip()

        if not ok:
            ans = int( out[0].rstrip().lstrip() )
            if ans == 0:
                return False
            elif ans == 1:
                return True
            else:
                raise Exception("Error when retrieving information about procedural languages from catalog")
        else:
            raise Exception("Error when retrieving information about procedural languages from catalog")

    def isInstalled(self, plname=None):
        """
        check if a given procedural language is installed on gpdb
        @param plname: the name of the procedural language to be checked
        @return: True if the given procedural language is installed on gpdb, False if it is not installed
        """
        if plname is None:
            plname = ""
        else:
            plname = plname.lower()

        if self.isSupported(plname):
            sql = "SELECT COUNT(*) FROM pg_language WHERE lanname='%s';" % (plname)

            cmd = PSQL(sql_cmd = sql, flags = '-q -t', dbname=os.environ.get('PGDATABASE'))
            tinctest.logger.info("Running command - %s" %cmd)
            cmd.run(validateAfter = False)
            result = cmd.get_results()
            ok = result.rc
            out = result.stdout.strip()

            print 'ok is', ok
            if not ok:
                print 'out is', out
                ans = int( out[0].rstrip().lstrip() )
                print 'ans is', ans
                if ans == 0:
                    return False
                elif ans == 1:
                    return True
                else:
                    raise Exception("Error when retrieving information about procedural languages from catalog")
            else:
                raise Exception("Error when retrieving information about procedural languages from catalog")
        else:
            raise Exception("Unsupported procedural language %s" % (plname))

    def installPL(self, plname=None):
        """
        install a given procedural language on gpdb
        @param plname: the name of the procedural language to be installed
        @return: True if the given procedural language is installed on gpdb or it already exists, False if it is not correctly installed
        """
        if plname is None:
            plname = ""
        else:
            plname = plname.lower()

        if self.isSupported(plname):
            if not self.isInstalled(plname):
                sql = "CREATE LANGUAGE %s;" % (plname)
                cmd = PSQL(sql_cmd = sql, flags = '-q -t', dbname=os.environ.get('PGDATABASE'))
                tinctest.logger.info("Running command - %s" %cmd)
                cmd.run(validateAfter = False)
                result = cmd.get_results()
                ok = result.rc
                out = result.stdout.strip()

                if not ok:
                    return True
                else:
                    print out
                    return False
            else:
                return True
        else:
            raise Exception("Unsupported procedural language %s" % (plname))

    def dropPL(self, plname=None):
        """
        drop a given procedural language on gpdb
        @param plname: the name of the procedural language to be dropped
        @return: True if the given procedural language is dropped on gpdb or it does not exist, False if it is not correctly dropped
        """ 
        if plname is None:
            plname = ""
        else:
            plname = plname.lower()

        if self.isSupported(plname):
            if self.isInstalled(plname):
                sql = "DROP LANGUAGE %s;" % (plname)

                cmd = PSQL(sql_cmd = sql, flags = '-q -t', dbname=os.environ.get('PGDATABASE'))
                tinctest.logger.info("Running command - %s" %cmd)
                cmd.run(validateAfter = False)
                result = cmd.get_results()
                ok = result.rc
                out = result.stdout.strip()

                if not ok:
                    return True
                else:
                    print out
                    return False
            else:
                return True
        else:
            raise Exception("Unsupported procedural language %s" % (plname))


if __name__ == "__main__":
    pl = gp_procedural_languages()

