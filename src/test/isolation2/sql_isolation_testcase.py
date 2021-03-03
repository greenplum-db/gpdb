"""
Copyright (c) 2004-Present VMware, Inc. or its affiliates.

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

import pg
import os
import subprocess
import re
import multiprocessing
import tempfile
import time
import sys
import socket
from optparse import OptionParser
import traceback

def is_digit(n):
    try:
        int(n)
        return True
    except ValueError:
        return  False

def null_notice_receiver(notice):
    '''
        Tests ignore notice messages when analyzing results,
        so silently drop notices from the pg.connection
    '''
    return


class SQLIsolationExecutor(object):
    def __init__(self, dbname=''):
        self.processes = {}
        # The re.S flag makes the "." in the regex match newlines.
        # When matched against a command in process_command(), all
        # lines in the command are matched and sent as SQL query.
        self.command_pattern = re.compile(r"^(-?\d+|[*])([&\\<\\>USq]*?)\:(.*)", re.S)
        if dbname:
            self.dbname = dbname
        else:
            self.dbname = os.environ.get('PGDATABASE')

    class SQLConnection(object):
        def __init__(self, out_file, name, mode, dbname):
            self.name = name
            self.mode = mode
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
                self.mode, pipe, self.dbname)
            sp.do()

        def query(self, command):
            print(file=self.out_file)
            self.out_file.flush()
            if len(command.strip()) == 0:
                return
            if self.has_open:
                raise Exception("Cannot query command while waiting for results")

            self.pipe.send((command, False))
            r = self.pipe.recv()
            if r is None:
                raise Exception("Execution failed")
            print(r.rstrip(), file=self.out_file)

        def fork(self, command, blocking):
            print("  <waiting ...>", file=self.out_file)
            self.pipe.send((command, True))

            if blocking:
                time.sleep(0.5)
                if self.pipe.poll(0):
                    p = self.pipe.recv()
                    raise Exception("Forked command is not blocking; got output: %s" % p.strip())
            self.has_open = True

        def join(self):
            r = None
            print("  <... completed>", file=self.out_file)
            if self.has_open:
                r = self.pipe.recv()
            if r is None:
                raise Exception("Execution failed")
            print(r.rstrip(), file=self.out_file)
            self.has_open = False

        def stop(self):
            self.pipe.send(("", False))
            self.p.join()
            if self.has_open:
                raise Exception("Should not finish test case while waiting for results")

        def quit(self):
            print(" ... <quitting>", file=self.out_file)
            self.stop()
        
        def terminate(self):
            self.pipe.close()
            self.p.terminate()

    class SQLSessionProcess(object):
        def __init__(self, name, mode, pipe, dbname):
            """
                Constructor
            """
            self.name = name
            self.mode = mode
            self.pipe = pipe
            self.dbname = dbname
            if self.mode == "utility":
                (hostname, port) = self.get_hostname_port(name, 'p')
                self.con = self.connectdb(given_dbname=self.dbname,
                                          given_host=hostname,
                                          given_port=port,
                                          given_opt="-c gp_role=utility")
            elif self.mode == "standby":
                # Connect to standby even when it's role is recorded
                # as mirror.  This is useful for scenarios where a
                # test needs to promote a standby without using
                # gpactivatestandby.
                (hostname, port) = self.get_hostname_port(name, 'm')
                self.con = self.connectdb(given_dbname=self.dbname,
                                          given_host=hostname,
                                          given_port=port)
            else:
                self.con = self.connectdb(self.dbname)

        def connectdb(self, given_dbname, given_host = None, given_port = None, given_opt = None):
            con = None
            retry = 1000
            while retry:
                try:
                    if (given_port is None):
                        con = pg.connect(host= given_host,
                                          opt= given_opt,
                                          dbname= given_dbname)
                    else:
                        con = pg.connect(host= given_host,
                                                  port= given_port,
                                                  opt= given_opt,
                                                  dbname= given_dbname)
                    break
                except Exception as e:
                    if (("the database system is starting up" in str(e) or
                         "the database system is in recovery mode" in str(e)) and
                        retry > 1):
                        retry -= 1
                        time.sleep(0.1)
                    else:
                        raise
            con.set_notice_receiver(null_notice_receiver)
            return con

        def get_hostname_port(self, contentid, role):
            """
                Gets the port number/hostname combination of the
                contentid and role
            """
            query = ("SELECT hostname, port FROM gp_segment_configuration WHERE"
                     " content = %s AND role = '%s'") % (contentid, role)
            con = self.connectdb(self.dbname, given_opt="-c gp_role=utility")
            r = con.query(query).getresult()
            con.close()
            if len(r) == 0:
                raise Exception("Invalid content %s" % contentid)
            if r[0][0] == socket.gethostname():
                return (None, int(r[0][1]))
            return (r[0][0], int(r[0][1]))

        def printout_result(self, r):
            """
            Print out a pygresql result set (a Query object, after the query
            has been executed), in a format that imitates the default
            formatting of psql. This isn't a perfect imitation: we left-justify
            all the fields and headers, whereas psql centers the header, and
            right-justifies numeric fields. But this is close enough, to make
            gpdiff.pl recognize the result sets as such. (We used to just call
            str(r), and let PyGreSQL do the formatting. But even though
            PyGreSQL's default formatting is close to psql's, it's not close
            enough.)
            """
            widths = []

            # Figure out the widths of each column.
            fields = r.listfields()
            for f in fields:
                widths.append(len(str(f)))

            rset = r.getresult()
            for row in rset:
                colno = 0
                for col in row:
                    if col is None:
                        col = ""
                    widths[colno] = max(widths[colno], len(str(col)))
                    colno = colno + 1

            # Start printing. Header first.
            result = ""
            colno = 0
            for f in fields:
                if colno > 0:
                    result += "|"
                result += " " + f.ljust(widths[colno]) + " "
                colno = colno + 1
            result += "\n"

            # Then the bar ("----+----")
            colno = 0
            for f in fields:
                if colno > 0:
                    result += "+"
                result += "".ljust(widths[colno] + 2, "-")
                colno = colno + 1
            result += "\n"

            # Then the result set itself
            for row in rset:
                colno = 0
                for col in row:
                    if colno > 0:
                        result += "|"
                    if isinstance(col, float):
                        col = format(col, "g")
                    elif isinstance(col, bool):
                        if col:
                            col = 't'
                        else:
                            col = 'f'
                    elif col is None:
                        col = ""
                    result += " " + str(col).ljust(widths[colno]) + " "
                    colno = colno + 1
                result += "\n"

            # Finally, the row count
            if len(rset) == 1:
                result += "(1 row)\n"
            else:
                result += "(" + str(len(rset)) + " rows)\n"

            return result

        def execute_command(self, command):
            """
                Executes a given command
            """
            try:
                r = self.con.query(command)
                if r is not None:
                    if type(r) == str:
                        # INSERT, UPDATE, etc that returns row count but not result set
                        echo_content = command[:-1].partition(" ")[0].upper()
                        return "%s %s" % (echo_content, r)
                    else:
                        # SELECT or similar, print the result set without the command (type pg.Query)
                        return self.printout_result(r)
                else:
                    # CREATE or other DDL without a result set or count
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


    def get_process(self, out_file, name, mode="", dbname=""):
        """
            Gets or creates the process by the given name
        """
        if len(name) > 0 and not is_digit(name):
            raise Exception("Name should be a number")
        if len(name) > 0 and mode != "utility" and int(name) >= 1024:
            raise Exception("Session name should be smaller than 1024 unless it is utility mode number")

        if not (name, mode) in self.processes:
            if not dbname:
                dbname = self.dbname
            self.processes[(name, mode)] = SQLIsolationExecutor.SQLConnection(out_file, name, mode, dbname)
        return self.processes[(name, mode)]

    def quit_process(self, out_file, name, mode="", dbname=""):
        """
        Quits a process with the given name
        """
        if len(name) > 0 and not is_digit(name):
            raise Exception("Name should be a number")
        if len(name) > 0 and mode != "utility" and int(name) >= 1024:
            raise Exception("Session name should be smaller than 1024 unless it is utility mode number")

        if not (name, mode) in self.processes:
            raise Exception("Sessions not started cannot be quit")

        self.processes[(name, mode)].quit()
        del self.processes[(name, mode)]

    def get_all_primary_contentids(self, dbname):
        """
        Retrieves all primary content IDs (including the master). Intended for
        use by *U queries.
        """
        if not dbname:
            dbname = self.dbname

        con = pg.connect(dbname=dbname)
        result = con.query("SELECT content FROM gp_segment_configuration WHERE role = 'p'").getresult()
        if len(result) == 0:
            raise Exception("Invalid gp_segment_configuration contents")
        return [int(content[0]) for content in result]

    def process_command(self, command, output_file):
        """
            Processes the given command.
            The command at this point still includes the isolation behavior
            flags, e.g. which session to use.
        """
        process_name = ""
        sql = command
        flag = ""
        con_mode = ""
        dbname = ""
        m = self.command_pattern.match(command)
        if m:
            process_name = m.groups()[0]
            flag = m.groups()[1]
            if flag and flag[0] == "U":
                con_mode = "utility"
            elif flag and flag[0] == "S":
                if len(flag) > 1:
                    flag = flag[1:]
                con_mode = "standby"
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
                print(file=output_file)
                if mode == '\\retcode':
                    print('-- start_ignore', file=output_file)
                print(stdout.decode(), file=output_file)
                if mode == '\\retcode':
                    print('-- end_ignore', file=output_file)
                    print('(exited with code {})'.format(cmd_output.returncode), file=output_file)
            else:
                self.get_process(output_file, process_name, con_mode, dbname=dbname).query(sql.strip())
        elif flag == "&":
            self.get_process(output_file, process_name, con_mode, dbname=dbname).fork(sql.strip(), True)
        elif flag == ">":
            self.get_process(output_file, process_name, con_mode, dbname=dbname).fork(sql.strip(), False)
        elif flag == "<":
            if len(sql) > 0:
                raise Exception("No query should be given on join")
            self.get_process(output_file, process_name, con_mode, dbname=dbname).join()
        elif flag == "q":
            if len(sql) > 0:
                raise Exception("No query should be given on quit")
            self.quit_process(output_file, process_name, con_mode, dbname=dbname)
        elif flag == "U":
            if process_name == '*':
                process_names = [str(content) for content in self.get_all_primary_contentids(dbname)]
            else:
                process_names = [process_name]

            for name in process_names:
                self.get_process(output_file, name, con_mode, dbname=dbname).query(sql.strip())
        elif flag == "U&":
            self.get_process(output_file, process_name, con_mode, dbname=dbname).fork(sql.strip(), True)
        elif flag == "U<":
            if len(sql) > 0:
                raise Exception("No query should be given on join")
            self.get_process(output_file, process_name, con_mode, dbname=dbname).join()
        elif flag == "Uq":
            if len(sql) > 0:
                raise Exception("No query should be given on quit")
            self.quit_process(output_file, process_name, con_mode, dbname=dbname)
        elif flag == "S":
            self.get_process(output_file, process_name, con_mode, dbname=dbname).query(sql.strip())
        else:
            raise Exception("Invalid isolation flag")

    def process_isolation_file(self, sql_file, output_file):
        """
            Processes the given sql file and writes the output
            to output file
        """
        try:
            command = ""
            newline = False
            for line in sql_file:
                # this logic replicates the python2 behavior of a trailing comma at the end of print
                # i.e. ''' print >>output_file, line.strip(), '''
                print((" " if command and not newline else "") + line.strip(), end="", file=output_file)
                newline = False
                if line[0] == "!":
                    command_part = line # shell commands can use -- for multichar options like --include
                elif re.match(r";.*--", line) or re.match(r"^--", line):
                    command_part = line.partition("--")[0] # remove comment from line
                else:
                    command_part = line
                if command_part == "" or command_part == "\n":
                    print(file=output_file) 
                    newline = True
                elif re.match(r".*;\s*$", command_part) or re.match(r"^\d+[q\\<]:$", line) or re.match(r"^-?\d+[SU][q\\<]:$", line):
                    command += command_part
                    try:
                        self.process_command(command, output_file)
                    except Exception as e:
                        print("FAILED: ", e, file=output_file)
                    command = ""
                else:
                    command += command_part

            for process in list(self.processes.values()):
                process.stop()
        except:
            for process in list(self.processes.values()):
                process.terminate()
            raise
        finally:
            for process in list(self.processes.values()):
                process.terminate()

class SQLIsolationTestCase:
    """
        The isolation test case allows a fine grained control of interleaved
        executing transactions. This is mainly used to test isolation behavior.

        [<#>[flag]:] <sql> | ! <shell scripts or command>
        #: either an integer indicating a unique session, or a content-id if
           followed by U (for utility-mode connections). In 'U' mode, the
           content-id can alternatively be an asterisk '*' to perform a
           utility-mode query on the master and all primaries.
        flag:
            &: expect blocking behavior
            >: running in background without blocking
            <: join an existing session
            q: quit the given session

            U: connect in utility mode to primary contentid from gp_segment_configuration
            U&: expect blocking behavior in utility mode (does not currently support an asterisk target)
            U<: join an existing utility mode session (does not currently support an asterisk target)

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

        Catalog Modification:

        Some tests are easier to write if it's possible to modify a system
        catalog across the *entire* cluster. To perform a utility-mode query on
        all segments and the master, you can use *U commands:

        *U: SET allow_system_table_mods = true;
        *U: UPDATE pg_catalog.<table> SET <column> = <value> WHERE <cond>;

        Since the number of query results returned by a *U command depends on
        the developer's cluster configuration, it can be useful to wrap them in
        a start_/end_ignore block. (Unfortunately, this also hides legitimate
        failures; a better long-term solution is needed.)

        Block/join flags are not currently supported with *U.

        Line continuation:
        If a line is not ended by a semicolon ';' which is followed by 0 or more spaces, the line will be combined with next line and
        sent together as a single statement.

        e.g.: Send to the server separately:
        1: SELECT * FROM t1; -> send "SELECT * FROM t1;"
        SELECT * FROM t2; -> send "SELECT * FROM t2;"

        e.g.: Send to the server once:
        1: SELECT * FROM
        t1; SELECT * FROM t2; -> "send SELECT * FROM t1; SELECT * FROM t2;"

        ATTENTION:
        Send multi SQL statements once:
        Multi SQL statements can be sent at once, but there are some known issues. Generally only the last query result will be printed.
        But due to the difficulties of dealing with semicolons insides quotes, we always echo the first SQL command instead of the last
        one if query() returns None. This created some strange issues like:

        CREATE TABLE t1 (a INT); INSERT INTO t1 SELECT generate_series(1,1000);
        CREATE 1000 (Should be INSERT 1000, but here the CREATE is taken due to the limitation)
    """

    def run_sql_file(self, sql_file, out_file = None, out_dir = None, optimizer = None):
        """
        Given a sql file and an ans file, this adds the specified gucs (self.gucs) to the sql file , runs the sql
        against the test case database (self.db_name) and verifies the output with the ans file.
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
