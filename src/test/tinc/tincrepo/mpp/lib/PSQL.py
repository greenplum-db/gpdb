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

from gppylib.commands.base import Command

import inspect
import os
import time

import tinctest

from tinctest.main import TINCException
from tinctest.lib.timeout import Timeout

class PSQLException(TINCException):
    pass

class PSQL(Command):

    """This is a wrapper for running sql command."""
    def __init__(self, sql_file = None, sql_cmd = None, out_file = None, output_to_file = True, 
                 dbname = None,host = None, port = None, username = None, password = None,
                 PGOPTIONS = None, flags = '-a', isODBC = None,
                 timeout = 900, background = False):

        PSQL.propagate_env_map = {}

        if not dbname:
            dbname_option = ""
        else:
            dbname_option = "-d %s" % (dbname)

        if not username:
            username_option = ""
        else:
            username_option = "-U %s" % (username)

        if password:
            PSQL.propagate_env_map = {'PGPASSWORD': password}

        if not PGOPTIONS:
            PGOPTIONS = ""
        else:
            PGOPTIONS = "PGOPTIONS='%s'" % PGOPTIONS

        if not host:
            hostname_option = ""
        else:
            hostname_option = "-h %s" % (host)

        if not port:
            port_option = ""
        else:
            port_option = "-p %s" % (port)

        if sql_file:
            if not os.path.exists(sql_file):
                raise PSQLException('SQL file %s does not exist. ' %sql_file)

            cmd_str = '%s psql --pset pager=off %s %s %s %s %s -f %s' \
                % (PGOPTIONS, dbname_option, username_option, hostname_option, port_option,
                   flags, sql_file)

            if not out_file:
                out_file = sql_file.replace('.sql', '.out')
        
            if output_to_file:
                cmd_str = "%s &> %s 2>&1" % (cmd_str, out_file)
        else:
            assert sql_cmd is not None
            cmd_str = "%s psql --pset pager=off %s %s %s %s %s -c \"%s\"" \
                      % (PGOPTIONS,dbname_option,username_option,hostname_option,
                         port_option, flags, sql_cmd)

            if output_to_file and out_file:
                cmd_str = "%s &> %s 2>&1" % (cmd_str, out_file)
                
        if background:
            cmd_str = "%s &" %cmd_str

        Command.__init__(self, 'run sql', cmd_str)

    @staticmethod
    def run_sql_file(sql_file, out_file = None, output_to_file = True,
                     dbname = None, host = None, port = None, username = None, password = None,
                     PGOPTIONS = None, flags = '-a', isODBC = None,
                     timeout = 900, background = False):
        """
        Run the given sql file using psql command line.

        @type sql_file: string
        @param sql_file: Complete path to the sql file.

        @type out_file: string
        @param out_file: Capture the output to a file if given.

        @type dbname: string
        @param dbname: Database against which the sql will be run.

        @type host: string
        @param host: Hostname for the connection

        @type port: integer
        @param port: Port number for the connection

        @type username: string
        @param username: Username for the connection

        @type password: string
        @param password: Password for the connection. Will be set through PGPASSWORD env variable for the connection.

        @type PGOPTIONS: string
        @param PGOPTIONS: Additional configurations for the connection.

        @type flags: string
        @param flags: PSQL flags to be used. Defaults to '-a'

        @type isODBC: boolean
        @param isODBC: Use ODBC for the connection if set.

        @type timeout: integer
        @param timeout: Timeout in seconds for the command line after which the command will be terminated and this will raise a L{PSQLException}

        @type background: boolean
        @param background: Run the command in the background and return immediately if set.

        @rtype: boolean
        @return: True if the command invocation was successful. False otherwise.

        @raise PSQLException: When the sql file cannot be located.
        """
        cmd = PSQL(sql_file = sql_file, out_file = out_file, output_to_file = output_to_file,
                   dbname = dbname, host = host, port = port, username = username, password = password,
                   PGOPTIONS = PGOPTIONS, flags = flags,
                   isODBC = isODBC, timeout = timeout, background = background)
        tinctest.logger.debug("Running sql file - %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        tinctest.logger.debug("Output - %s" %result)
        if result.rc != 0:
            return False
        return True

    @staticmethod
    def run_sql_command(sql_cmd, out_file = None, dbname = None,
                        host = None, port = None, username = None, password = None,
                        PGOPTIONS = None, flags = '-a', isODBC = None,
                        timeout = 900, background = False, results={'rc':0, 'stdout':'', 'stderr':''}):
        """
        Run the given sql command using psql command line.

        @type sql_cmd: string
        @param sql_cmd: SQL command to run through the PSQL command line

        @type out_file: string
        @param out_file: Capture the output to a file if given.

        @type dbname: string
        @param dbname: Database against which the sql will be run.

        @type host: string
        @param host: Hostname for the connection

        @type port: integer
        @param port: Port number for the connection

        @type username: string
        @param username: Username for the connection

        @type password: string
        @param password: Password for the connection. Will be set through PGPASSWORD env variable for the connection.

        @type PGOPTIONS: string
        @param PGOPTIONS: Additional configurations for the connection.

        @type flags: string
        @param flags: PSQL flags to be used. Defaults to '-a'

        @type isODBC: boolean
        @param isODBC: Use ODBC for the connection if set.

        @type timeout: integer
        @param timeout: Timeout in seconds for the command line after which the command will be terminated and this will raise a L{PSQLException}

        @type background: boolean
        @param background: Run the command in the background and return immediately if set.

        @rtype: string
        @return: Output of the sql command
        """
        cmd = PSQL(sql_cmd = sql_cmd, out_file = out_file, dbname = dbname,
                   host = host, port = port, username = username, password = password,
                   PGOPTIONS = PGOPTIONS, flags = flags,
                   isODBC = isODBC, timeout = timeout, background = background)
        tinctest.logger.debug("Running command - %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        results['rc'] = result.rc
        results['stdout'] = result.stdout
        results['stderr'] = result.stderr
        tinctest.logger.debug("Output - %s" %result)
        return result.stdout

    @staticmethod
    def run_sql_file_utility_mode(sql_file, out_file = None, dbname = None,
                                  host = None, port = None, username = None, password = None,
                                  PGOPTIONS = None, flags = '-a', isODBC = None,
                                  timeout = 900, background = False, output_to_file=True):
        """
        Run the given sql file using psql command line in utility mode.

        @type sql_file: string
        @param sql_file: Complete path to the sql file.

        @type out_file: string
        @param out_file: Capture the output to a file if given.

        @type dbname: string
        @param dbname: Database against which the sql will be run.

        @type host: string
        @param host: Hostname for the connection

        @type port: integer
        @param port: Port number for the connection

        @type username: string
        @param username: Username for the connection

        @type password: string
        @param password: Password for the connection. Will be set through PGPASSWORD env variable for the connection.

        @type PGOPTIONS: string
        @param PGOPTIONS: Additional configurations for the connection.

        @type flags: string
        @param flags: PSQL flags to be used. Defaults to '-a'

        @type isODBC: boolean
        @param isODBC: Use ODBC for the connection if set.

        @type timeout: integer
        @param timeout: Timeout in seconds for the command line after which the command will be terminated and this will raise a L{PSQLException}

        @type background: boolean
        @param background: Run the command in the background and return immediately if set.

        @rtype: boolean
        @return: True if the command invocation was successful. False otherwise.

        @raise PSQLException: When the sql file cannot be located.
        """
        if PGOPTIONS:
            PGOPTIONS = PGOPTIONS + " -c gp_session_role=utility"
        else:
            PGOPTIONS =  "-c gp_session_role=utility"
        return PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, dbname = dbname,
                                 host = host, port = port,
                                 username = username, password = password,
                                 PGOPTIONS = PGOPTIONS, flags = flags,
                                 isODBC = isODBC, timeout = timeout, background = background,
                                 output_to_file=output_to_file)

    @staticmethod
    def run_sql_command_utility_mode(sql_cmd, out_file = None, dbname = None,
                                     host = None, port = None, username = None, password = None,
                                     PGOPTIONS = None, flags = '-a', isODBC = None,
                                     timeout = 900, background = False):
        """
        Run the given sql command using psql command line in utility mode.

        @type sql_cmd: string
        @param sql_cmd: SQL command to run through the PSQL command line

        @type out_file: string
        @param out_file: Capture the output to a file if given.

        @type dbname: string
        @param dbname: Database against which the sql will be run.

        @type host: string
        @param host: Hostname for the connection

        @type port: integer
        @param port: Port number for the connection

        @type username: string
        @param username: Username for the connection

        @type password: string
        @param password: Password for the connection. Will be set through PGPASSWORD env variable for the connection.

        @type PGOPTIONS: string
        @param PGOPTIONS: Additional configurations for the connection.

        @type flags: string
        @param flags: PSQL flags to be used. Defaults to '-a'

        @type isODBC: boolean
        @param isODBC: Use ODBC for the connection if set.

        @type timeout: integer
        @param timeout: Timeout in seconds for the command line after which the command will be terminated and this will raise a L{PSQLException}

        @type background: boolean
        @param background: Run the command in the background and return immediately if set.

        @rtype: string
        @return: Output of the sql command
        """
        if PGOPTIONS:
            PGOPTIONS = PGOPTIONS + " -c gp_session_role=utility"
        else:
            PGOPTIONS =  "-c gp_session_role=utility"
        return PSQL.run_sql_command(sql_cmd = sql_cmd, out_file = out_file, dbname = dbname,
                                    host = host, port = port, username = username, password = password,
                                    PGOPTIONS = PGOPTIONS, flags = flags,
                                    isODBC = isODBC, timeout = timeout, background = background)

    @staticmethod
    def run_sql_file_catalog_update(sql_file, out_file = None, dbname = None,
                                    host = None, port = None, username = None, password = None,
                                    PGOPTIONS = None, flags = '-a', isODBC = None,
                                    timeout = 900, background = False):
        """
        Run the given sql file using psql command line with catalog update privilege

        @type sql_file: string
        @param sql_file: Complete path to the sql file.

        @type out_file: string
        @param out_file: Capture the output to a file if given.

        @type dbname: string
        @param dbname: Database against which the sql will be run.

        @type host: string
        @param host: Hostname for the connection

        @type port: integer
        @param port: Port number for the connection

        @type username: string
        @param username: Username for the connection

        @type password: string
        @param password: Password for the connection. Will be set through PGPASSWORD env variable for the connection.

        @type PGOPTIONS: string
        @param PGOPTIONS: Additional configurations for the connection.

        @type flags: string
        @param flags: PSQL flags to be used. Defaults to '-a'

        @type isODBC: boolean
        @param isODBC: Use ODBC for the connection if set.

        @type timeout: integer
        @param timeout: Timeout in seconds for the command line after which the command will be terminated and this will raise a L{PSQLException}

        @type background: boolean
        @param background: Run the command in the background and return immediately if set.

        @rtype: boolean
        @return: True if the command invocation was successful. False otherwise.

        @raise PSQLException: When the sql file cannot be located.
        """
        if PGOPTIONS:
            PGOPTIONS = PGOPTIONS + " -c gp_session_role=utility -c allow_system_table_mods=dml"
        else:
            PGOPTIONS =  "-c gp_session_role=utility -c allow_system_table_mods=dml"
        return PSQL.run_sql_file(sql_file = sql_file, out_file = out_file, dbname = dbname,
                                 host = host, port = port,
                                 username = username, password = password,
                                 PGOPTIONS = PGOPTIONS, flags = flags,
                                 isODBC = isODBC, timeout = timeout, background = background)

    @staticmethod
    def run_sql_command_catalog_update(sql_cmd, out_file = None, dbname = None,
                                       host = None, port = None, username = None, password = None,
                                       PGOPTIONS = None, flags = '-a', isODBC = None,
                                       timeout = 900, background = False):
        """
        Run the given sql command using psql command line with catalog update.

        @type sql_cmd: string
        @param sql_cmd: SQL command to run through the PSQL command line

        @type out_file: string
        @param out_file: Capture the output to a file if given.

        @type dbname: string
        @param dbname: Database against which the sql will be run.

        @type host: string
        @param host: Hostname for the connection

        @type port: integer
        @param port: Port number for the connection

        @type username: string
        @param username: Username for the connection

        @type password: string
        @param password: Password for the connection. Will be set through PGPASSWORD env variable for the connection.

        @type PGOPTIONS: string
        @param PGOPTIONS: Additional configurations for the connection.

        @type flags: string
        @param flags: PSQL flags to be used. Defaults to '-a'

        @type isODBC: boolean
        @param isODBC: Use ODBC for the connection if set.

        @type timeout: integer
        @param timeout: Timeout in seconds for the command line after which the command will be terminated and this will raise a L{PSQLException}

        @type background: boolean
        @param background: Run the command in the background and return immediately if set.

        @rtype: string
        @return: Output of the sql command
        """
        if PGOPTIONS:
            PGOPTIONS = PGOPTIONS + " -c gp_session_role=utility -c allow_system_table_mods=dml"
        else:
            PGOPTIONS =  "-c gp_session_role=utility -c allow_system_table_mods=dml"
        return PSQL.run_sql_command(sql_cmd = sql_cmd, out_file = out_file, dbname = dbname,
                                    host = host, port = port, username = username, password = password,
                                    PGOPTIONS = PGOPTIONS, flags = flags,
                                    isODBC = isODBC, timeout = timeout, background = background)
    @staticmethod
    def drop_database(dbname, retries = 5, sleep_interval = 5):
        """
        Execute dropdb against the given database.

        @type dbname: string
        @param dbname: Name of the database to be deleted

        @type retires: integer
        @param retries: Number of attempts to drop the database.

        @type sleep_interval: integer
        @param sleep_interval: Time in seconds between retry attempts

        @rtype: boolean
        @return: True if successful, False otherwise

        @raise PSQLException: When the database does not exist
        """
        # TBD: Use shell when available
        if not PSQL.database_exists(dbname):
            tinctest.logger.error("Database %s does not exist." %dbname)
            raise PSQLException('Database %s does not exist' %dbname)
        cmd = Command(name='drop database', cmdStr='dropdb %s' %(dbname))
        tinctest.logger.debug("Dropping database: %s" %cmd)
        count = 0
        while count < retries:
            cmd.run(validateAfter = False)
            result = cmd.get_results()
            tinctest.logger.debug("Output - %s" %result)
            if result.rc == 0 and not result.stderr:
                return True
            time.sleep(sleep_interval)
            count += 1
        return False

    @staticmethod
    def create_database(dbname):
        """
        Create a database with the given database name.

        @type dbname: string
        @param dbname: Name of the database to be created

        @rtype: boolean
        @return: True if successful, False otherwise

        raise PSQLException: When the database already exists.
        """
        # TBD: Use shell when available
        if PSQL.database_exists(dbname):
            raise PSQLException("Database %s already exists" %dbname)
        cmd = Command(name='drop database', cmdStr='createdb %s' %(dbname))
        tinctest.logger.debug("Creating database: %s" %cmd)
        cmd.run(validateAfter = False)
        result = cmd.get_results()
        tinctest.logger.debug("Output - %s" %result)
        if result.rc != 0 or result.stderr:
            return False
        return True

    @staticmethod
    def reset_database(dbname, retries = 5, sleep_interval = 5):
        """
        Drops and recreates the database with the given database name

        @type dbname: string
        @param dbname: Name of the database

        @type retires: integer
        @param retries: Number of attempts to drop the database.

        @type sleep_interval: integer
        @param sleep_interval: Time in seconds between retry attempts

        @rtype: boolean
        @return: True if successful, False otherwise
        """
        if PSQL.database_exists(dbname):
            result = PSQL.drop_database(dbname, retries, sleep_interval)
            if not result:
                tinctest.logger.warning("Could not delete database %s" %dbname)
                return False
        return PSQL.create_database(dbname)


    @staticmethod
    def database_exists(dbname):
        """
        Inspects if the database with the given name exists.

        @type dbname: string
        @param dbname: Name of the database

        @rtype: boolean
        @return: True if the database exists, False otherwise
        """
        sql_cmd = "select 'command_found_' || datname from pg_database where datname like '" + dbname + "'"
        output = PSQL.run_sql_command(sql_cmd = sql_cmd)
        if 'command_found_' + dbname in output:
            return True
        return False

    @staticmethod
    def wait_for_database_up(dbname = None, host = None, port = None, username = None, password = None):
        '''
        Wait till the system is up, as master may take some time
        to come back after FI crash.
        '''
        down = True
        results = {'rc':0, 'stdout':'', 'stderr':''}
        # FIXME: Temporary work-around to let postmaster complete
        # shutdown sequence.  In some cases, upon a PANIC, the
        # postmaster may not receive a signal notifying child's death
        # until several seconds after the child died.  If we try to
        # run the following SQL before postmaster receives the signal,
        # we will incorrectly interpret that the database is up and
        # running.  Options for proper fix: (1) distributed XID is
        # reset to 0 upon restart/PANIC.  gp_distributed_xacts view
        # can be queried to check that.  But it will not work if we
        # are being called after a segment PANIC. (2) Checkpoint
        # information in pg_control can be used to ascertain that a
        # segment (master/primary/mirror) has performed startup.
        time.sleep(30)
        for i in range(60):
            time.sleep(1)
            res = PSQL.run_sql_command('select count(*) from gp_dist_random(\'gp_id\');',
                                dbname=dbname, host=host, port=port, username = username,
                                password=password, results=results)
            if results['rc'] == 0:
                down = False
                break

        if down:
            raise PSQLException('database has not come up')
