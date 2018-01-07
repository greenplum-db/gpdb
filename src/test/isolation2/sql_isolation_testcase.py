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

import pygresql.pg
import os
import subprocess
import re
import multiprocessing
import time
import sys
import socket
from optparse import OptionParser
import traceback

class SQLIsolationExecutor(object):
    def __init__(self, dbname=''):
        self.processes = {}
        self.command_pattern = re.compile(r"^(\d+)([&\\<\\>Uq]*?)\:(.*)")
        if dbname:
            self.dbname = dbname
        else:
            self.dbname = os.environ.get('PGDATABASE')

    class SQLConnection(object):
        def __init__(self, out_file, name, utility_mode, dbname):
            self.name = name
            self.utility_mode = utility_mode
            self.out_file = out_file
            self.dbname = dbname

            parent_conn, child_conn = multiprocessing.Pipe(True)
            self.p = multiprocessing.Process(target=self.session_process, args=(child_conn,))   
            self.pipe = parent_conn
            self.has_open = False
            self.p.start()

            # Close "our" copy of the child's handle, so that if the child dies,
            # recv() on the pipe will fail.
            child_conn.close();

            self.out_file = out_file

        def session_process(self, pipe):
            sp = SQLIsolationExecutor.SQLSessionProcess(self.name, 
                self.utility_mode, self.out_file.name, pipe, self.dbname)
            sp.do()

        def query(self, command):
            print >>self.out_file
            self.out_file.flush()
            if len(command.strip()) == 0:
                return
            if self.has_open:
                raise Exception("Cannot query command while waiting for results")

            self.pipe.send((command, False))
            r = self.pipe.recv()
            if r is None:
                raise Exception("Execution failed")
            print >>self.out_file, r.strip()

        def fork(self, command, blocking):
            print >>self.out_file, " <waiting ...>"
            self.pipe.send((command, True))

            if blocking:
                time.sleep(0.5)
                if self.pipe.poll(0):
                    p = self.pipe.recv()
                    raise Exception("Forked command is not blocking; got output: %s" % p.strip())
            self.has_open = True

        def join(self):
            r = None
            print >>self.out_file, " <... completed>"
            if self.has_open:
                r = self.pipe.recv()
            if r is None:
                raise Exception("Execution failed")
            print >>self.out_file, r.strip()
            self.has_open = False

        def stop(self):
            self.pipe.send(("", False))
            self.p.join()
            if self.has_open:
                raise Exception("Should not finish test case while waiting for results")

        def quit(self):
            print >>self.out_file, "... <quitting>"
            self.stop()
        
        def terminate(self):
            self.pipe.close()
            self.p.terminate()

    class SQLSessionProcess(object):
        def __init__(self, name, utility_mode, output_file, pipe, dbname):
            """
                Constructor
            """
            self.name = name
            self.utility_mode = utility_mode
            self.pipe = pipe
            self.dbname = dbname
            if self.utility_mode:
                (hostname, port) = self.get_utility_mode_port(name)
                self.con = pygresql.pg.connect(host=hostname, 
                    port=port, 
                    opt="-c gp_session_role=utility",
                    dbname=self.dbname)
            else:
                self.con = self.connectdb()

            self.filename = "%s.%s" % (output_file, os.getpid())

        def connectdb(self):
            con = None
            retry = 1000
            while retry:
                try:
                    con = pygresql.pg.connect(dbname=self.dbname)
                    break
                except Exception as e:
                    if (("the database system is starting up" in str(e) or
                         "the database system is in recovery mode" in str(e)) and
                        retry > 1):
                        retry -= 1
                    else:
                        raise
            return con

        def get_utility_mode_port(self, name):
            """
                Gets the port number/hostname combination of the
                dbid with the id = name
            """
            con = self.connectdb()
            r = con.query("SELECT hostname, port FROM gp_segment_configuration WHERE dbid = %s" % name).getresult()
            if len(r) == 0:
                raise Exception("Invalid dbid %s" % name)
            if r[0][0] == socket.gethostname():
                return (None, int(r[0][1]))
            return (r[0][0], int(r[0][1]))

        def printout_result(self, r):
            """
                This is a pretty dirty, but apprently the only way
                to get the pretty output of the query result.
                The reason is that for some python internal reason  
                print(r) calls the correct function while neighter str(r)
                nor repr(r) output something useful. 
            """
            with open(self.filename, "w") as f:
                print >>f, r,
                f.flush()   

            with open(self.filename, "r") as f:
                ppr = f.read()
                return ppr.strip() + "\n"

        def execute_command(self, command):
            """
                Executes a given command
            """
            try:
                r = self.con.query(command)
                if r and type(r) == str:
                    echo_content = command[:-1].partition(" ")[0].upper()
                    return "%s %s" % (echo_content, self.printout_result(r))
                elif r:
                    return self.printout_result(r)
                else:
                    echo_content = command[:-1].partition(" ")[0].upper()
                    return echo_content
            except Exception as e:
                return str(e)

        def do(self):
            """
                Process loop.
                Ends when the command None is received
            """
            (c, wait) = self.pipe.recv()
            while c:
                if wait:
                    time.sleep(0.1)
                r = self.execute_command(c)
                self.pipe.send(r)
                r = None

                (c, wait) = self.pipe.recv()

            if os.path.exists(self.filename):
                os.unlink(self.filename)

    def get_process(self, out_file, name, utility_mode=False, dbname=""):
        """
            Gets or creates the process by the given name
        """
        if len(name) > 0 and not name.isdigit():
            raise Exception("Name should be a number")
        if len(name) > 0 and not utility_mode and int(name) >= 1024:
            raise Exception("Session name should be smaller than 1024 unless it is utility mode number")

        if not (name, utility_mode) in self.processes:
            if not dbname:
                dbname = self.dbname
            self.processes[(name, utility_mode)] = SQLIsolationExecutor.SQLConnection(out_file, name, utility_mode, dbname)
        return self.processes[(name, utility_mode)]

    def quit_process(self, out_file, name, utility_mode=False, dbname=""):
        """
        Quits a process with the given name
        """
        if len(name) > 0 and not name.isdigit():
            raise Exception("Name should be a number")
        if len(name) > 0 and not utility_mode and int(name) >= 1024:
            raise Exception("Session name should be smaller than 1024 unless it is utility mode number")

        if not (name, utility_mode) in self.processes:
            raise Exception("Sessions not started cannot be quit")

        self.processes[(name, utility_mode)].quit()
        del self.processes[(name, utility_mode)]

    def process_command(self, command, output_file):
        """
            Processes the given command.
            The command at this point still includes the isolation behavior
            flags, e.g. which session to use.
        """
        process_name = ""
        sql = command
        flag = ""
        dbname = ""
        m = self.command_pattern.match(command)
        if m:
            process_name = m.groups()[0]
            flag = m.groups()[1]
            sql = m.groups()[2]
            sql = sql.lstrip()
            # If db_name is specifed , it should be of the following syntax:
            # 1:@db_name <db_name>: <sql>
            if sql.startswith('@db_name'):
                sql_parts = sql.split(':', 2)
                if not len(sql_parts) == 2:
                    raise Exception("Invalid syntax with dbname, should be of the form 1:@db_name <db_name>: <sql>")
                if not sql_parts[0].startswith('@db_name'):
                    raise Exception("Invalid syntax with dbname, should be of the form 1:@db_name <db_name>: <sql>")
                if not len(sql_parts[0].split()) == 2:
                    raise Exception("Invalid syntax with dbname, should be of the form 1:@db_name <db_name>: <sql>")
                dbname = sql_parts[0].split()[1].strip()
                if not dbname:
                    raise Exception("Invalid syntax with dbname, should be of the form 1:@db_name <db_name>: <sql>")
                sql = sql_parts[1]
        if not flag:
            if sql.startswith('!'):
                sql = sql[1:]

                # Check for execution mode. E.g.
                #     !\retcode path/to/executable --option1 --option2 ...
                #
                # At the moment, we only recognize the \retcode mode, which
                # ignores all program output in the diff (it's still printed)
                # and adds the return code.
                mode = None
                if sql.startswith('\\'):
                    mode, sql = sql.split(None, 1)
                    if mode != '\\retcode':
                        raise Exception('Invalid execution mode: {}'.format(mode))

                cmd_output = subprocess.Popen(sql.strip(), stderr=subprocess.STDOUT, stdout=subprocess.PIPE, shell=True)
                stdout, _ = cmd_output.communicate()
                print >> output_file
                if mode == '\\retcode':
                    print >> output_file, '-- start_ignore'
                print >> output_file, stdout
                if mode == '\\retcode':
                    print >> output_file, '-- end_ignore'
                    print >> output_file, '(exited with code {})'.format(cmd_output.returncode)
            else:
                self.get_process(output_file, process_name, dbname=dbname).query(sql.strip())
        elif flag == "&":
            self.get_process(output_file, process_name, dbname=dbname).fork(sql.strip(), True)
        elif flag == ">":
            self.get_process(output_file, process_name, dbname=dbname).fork(sql.strip(), False)
        elif flag == "<":
            if len(sql) > 0:
                raise Exception("No query should be given on join")
            self.get_process(output_file, process_name, dbname=dbname).join()
        elif flag == "q":
            if len(sql) > 0:
                raise Exception("No query should be given on quit")
            self.quit_process(output_file, process_name, dbname=dbname)
        elif flag == "U":
            self.get_process(output_file, process_name, utility_mode=True, dbname=dbname).query(sql.strip())
        elif flag == "U&":
            self.get_process(output_file, process_name, utility_mode=True, dbname=dbname).fork(sql.strip(), True)
        elif flag == "U<":
            if len(sql) > 0:
                raise Exception("No query should be given on join")
            self.get_process(output_file, process_name, utility_mode=True, dbname=dbname).join()
        elif flag == "Uq":
            if len(sql) > 0:
                raise Exception("No query should be given on quit")
            self.quit_process(output_file, process_name, utility_mode=True, dbname=dbname)
        else:
            raise Exception("Invalid isolation flag")

    def process_isolation_file(self, sql_file, output_file):
        """
            Processes the given sql file and writes the output
            to output file
        """
        try:
            command = ""
            for line in sql_file:
                #tinctest.logger.info("re.match: %s" %re.match(r"^\d+[q\\<]:$", line))
                print >>output_file, line.strip(),
                if line[0] == "!":
                    command_part = line # shell commands can use -- for multichar options like --include
                else:
                    command_part = line.partition("--")[0] # remove comment from line
                if command_part == "" or command_part == "\n":
                    print >>output_file 
                elif command_part.endswith(";\n") or re.match(r"^\d+[q\\<]:$", line) or re.match(r"^\d+U[q\\<]:$", line):
                    command += command_part
                    try:
                        self.process_command(command, output_file)
                    except Exception as e:
                        print >>output_file, "FAILED: ", e
                    command = ""
                else:
                    command += command_part

            for process in self.processes.values():
                process.stop()
        except:
            for process in self.processes.values():
                process.terminate()
            raise
        finally:
            for process in self.processes.values():
                process.terminate()

class SQLIsolationTestCase:
    """
        The isolation test case allows a fine grained control of interleaved
        executing transactions. This is mainly used to test isolation behavior.

        [<#>[flag]:] <sql> | ! <shell scripts or command>
        #: just any integer indicating an unique session or dbid if followed by U
        flag:
            &: expect blocking behavior
            >: running in background without blocking
            <: join an existing session
            q: quit the given session

            U: connect in utility mode to dbid from gp_segment_configuration
            U&: expect blocking behavior in utility mode
            U<: join an existing utility mode session

        An example is:

        Execute BEGIN in transaction 1
        Execute BEGIN in transaction 2
        Execute INSERT in transaction 2
        Execute SELECT in transaction 1
        Execute COMMIT in transaction 2
        Execute SELECT in transaction 1

        The isolation tests are specified identical to sql-scripts in normal
        SQLTestCases. However, it is possible to prefix a SQL line with
        an tranaction identifier followed by a colon (":").
        The above example would be defined by
        1: BEGIN;
        2: BEGIN;
        2: INSERT INTO a VALUES (1);
        1: SELECT * FROM a;
        2: COMMIT;
        1: SELECT * FROM a;

        Blocking behavior can be tested by forking and joining.
        1: BEGIN;
        2: BEGIN;
        1: DELETE FROM foo WHERE a = 4;
        2&: DELETE FROM foo WHERE a = 4;
        1: COMMIT;
        2<:
        2: COMMIT;

        2& forks the command. It is executed in the background. If the
        command is NOT blocking at this point, it is considered an error.
        2< joins the background command and outputs the result of the   
        command execution.

        Session ids should be smaller than 1024.

        2U: Executes a utility command connected to port 40000. 

        One difference to SQLTestCase is the output of INSERT.
        SQLTestCase would output "INSERT 0 1" if one tuple is inserted.
        SQLIsolationTestCase would output "INSERT 1". As the
        SQLIsolationTestCase needs to have a more fine-grained control
        over the execution order than possible with PSQL, it uses
        the pygresql python library instead.

        Connecting to a specific database:
        1. If you specify a db_name metadata in the sql file, connect to that database in all open sessions.
        2. If you want a specific session to be connected to a specific database , specify the sql as follows:

        1:@db_name testdb: <sql>
        2:@db_name test2db: <sql>
        1: <sql>
        2: <sql>
        etc

        Here session 1 will be connected to testdb and session 2 will be connected to test2db. You can specify @db_name only at the beginning of the session. For eg:, following would error out:

        1:@db_name testdb: <sql>
        2:@db_name test2db: <sql>
        1: @db_name testdb: <sql>
        2: <sql>
        etc

        Quitting sessions:
        By default, all opened sessions will be stopped only at the end of the sql file execution. If you want to explicitly quit a session
        in the middle of the test execution, you can specify a flag 'q' with the session identifier. For eg:

        1:@db_name testdb: <sql>
        2:@db_name test2db: <sql>
        1: <sql>
        2: <sql>
        1q:
        2: <sql>
        3: <sql>
        2q:
        3: <sql>
        2: @db_name test: <sql>

        1q:  ---> Will quit the session established with testdb.
        2q:  ---> Will quit the session established with test2db.

        The subsequent 2: @db_name test: <sql> will open a new session with the database test and execute the sql against that session.
    """

    def run_sql_file(self, sql_file, out_file = None, out_dir = None, optimizer = None):
        """
        Given a sql file and an ans file, this adds the specified gucs (self.gucs) to the sql file , runs the sql
        against the test case databse (self.db_name) and verifies the output with the ans file.
        If an 'init_file' exists in the same location as the sql_file, this will be used
        while doing gpdiff.
        """
        # Add gucs to the test sql and form the actual sql file to be run
        if not out_dir:
            out_dir = self.get_out_dir()
            
        if not os.path.exists(out_dir):
            TINCSystem.make_dirs(out_dir, ignore_exists_error = True)
            
        if optimizer is None:
            gucs_sql_file = os.path.join(out_dir, os.path.basename(sql_file))
        else:
            # sql file will be <basename>_opt.sql or <basename>_planner.sql based on optimizer
            gucs_sql_file = os.path.join(out_dir, os.path.basename(sql_file).replace('.sql', '_%s.sql' %self._optimizer_suffix(optimizer)))
            
        self._add_gucs_to_sql_file(sql_file, gucs_sql_file, optimizer)
        self.test_artifacts.append(gucs_sql_file)

        
        if not out_file:
            if optimizer is None:
                out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '.out'))
            else:
                # out file will be *_opt.out or *_planner.out based on optimizer
                out_file = os.path.join(self.get_out_dir(), os.path.basename(sql_file).replace('.sql', '_%s.out' %self._optimizer_suffix(optimizer)))
        
        self.test_artifacts.append(out_file)
        executor = SQLIsolationExecutor(dbname=self.db_name)
        with open(out_file, "w") as f:
            executor.process_isolation_file(open(sql_file), f)
            f.flush()   
        
        if out_file[-2:] == '.t':
            out_file = out_file[:-2]

        return out_file

if __name__ == "__main__":

    parser = OptionParser()
    parser.add_option("--dbname", dest="dbname",
                      help="connect to database DBNAME", metavar="DBNAME")
    (options, args) = parser.parse_args()

    executor = SQLIsolationExecutor(dbname=options.dbname)

    executor.process_isolation_file(sys.stdin, sys.stdout)
