#!/usr/bin/env python3
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
import pty
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
import select

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


class ConnectionInfo(object):
    __instance = None

    def __init__(self):
        self.max_content_id = 0
        if ConnectionInfo.__instance is not None:
            raise Exception("ConnectionInfo is a singleton.")

        query = ("SELECT content, hostname, port, role FROM gp_segment_configuration")

        con = pg.connect(dbname="postgres")
        self._conn_map = con.query(query).getresult()
        con.close()

        ConnectionInfo.__instance = self
        for content, _, _, _ in ConnectionInfo.__instance._conn_map:
            if content >= self.max_content_id:
                self.max_content_id = content + 1

    @staticmethod
    def __get_instance():
        if ConnectionInfo.__instance is None:
            return ConnectionInfo()
        return ConnectionInfo.__instance

    @staticmethod
    def get_hostname_port(name, role_name):
        content_id = int(name)
        conn_map = ConnectionInfo.__get_instance()._conn_map
        max_content_id = ConnectionInfo.__get_instance().max_content_id
        real_content_id = content_id % max_content_id if content_id >= 0 else content_id % (-max_content_id)
        for content, host, port, role in conn_map:
            if real_content_id == content and role == role_name:
                return (host, port)
        raise Exception("Cannot find a connection with content_id=%d, role=%c" % (content_id, role_name))


class GlobalShellExecutor(object):
    BASH_PS1 = 'GlobalShellExecutor>'

    class ExecutionError(Exception):
        ""
        pass

    def __init__(self, output_file='', initfile_prefix=''):
        self.output_file = output_file
        self.initfile_prefix = initfile_prefix
        self.v_cnt = 0
        # open pseudo-terminal to interact with subprocess
        self.master_fd, self.slave_fd = pty.openpty()
        self.sh_proc = subprocess.Popen(['/bin/bash', '--noprofile', '--norc', '--noediting', '-i'],
                                        stdin=self.slave_fd,
                                        stdout=self.slave_fd,
                                        stderr=self.slave_fd,
                                        start_new_session=True,
                                        universal_newlines=True)
        self.bash_log_file = open("%s.log" % self.initfile_prefix, "w+")
        self.__run_command("export PS1='%s'" % GlobalShellExecutor.BASH_PS1)
        self.__run_command("export PS2=''")
        self.__run_command("source global_sh_executor.sh")

    def terminate(self, with_error = False):
        if self.sh_proc == None:
            return
        # If write the matchsubs section directly to the output, the generated token id will be compared by gpdiff.pl
        # so here just write all matchsubs section into an auto generated init file when this test case file finished.
        if not with_error and self.initfile_prefix != None and len(self.initfile_prefix) > 1:
            output_init_file = "%s.initfile" % self.initfile_prefix
            cmd = ''' [ ! -z "${MATCHSUBS}" ] && echo "-- start_matchsubs ${NL} ${MATCHSUBS} ${NL}-- end_matchsubs" > %s ''' % output_init_file
            self.exec_global_shell(cmd, False)

        if self.bash_log_file:
            self.bash_log_file.close()
        try:
            self.sh_proc.terminate()
        except OSError as e:
            # Log then ignore the exception if the process doesn't exist.
            print("Failed to terminate bash process: %s" % e)
            pass
        self.sh_proc = None

    def __run_command(self, sh_cmd):
        # Strip extra new lines
        sh_cmd = sh_cmd.rstrip()
        os.write(self.master_fd, sh_cmd.encode()+b'\n')

        output = ""
        while self.sh_proc.poll() is None:
            # If times out, consider it as an fatal error.
            r, _, e = select.select([self.master_fd], [], [self.master_fd], 30)
            if e:
                # Terminate the shell when we get any output from stderr
                o = os.read(self.master_fd, 10240)
                self.bash_log_file.write(o)
                self.bash_log_file.flush()
                self.terminate(True)
                raise GlobalShellExecutor.ExecutionError("Error happened to the bash process, see %s for details." % self.bash_log_file.name)

            if r:
                o = os.read(self.master_fd, 10240).decode()
                self.bash_log_file.write(o)
                self.bash_log_file.flush()
                output += o
                if output.endswith(GlobalShellExecutor.BASH_PS1):
                    lines = output.splitlines()
                    return lines[len(sh_cmd.splitlines()):len(lines) - 1]

            if not r and not e:
                self.terminate(True)
                raise GlobalShellExecutor.ExecutionError("Timeout happened to the bash process, see %s for details." % self.bash_log_file.name)

        self.terminate(True)
        raise GlobalShellExecutor.ExecutionError("Bash process has been stopped, see %s for details." % self.bash_log_file.name)

    # execute global shell cmd in bash process, and fetch result without blocking
    def exec_global_shell(self, sh_cmd, is_trip_output_end_blanklines):
        if self.sh_proc == None:
            raise GlobalShellExecutor.ExecutionError("The bash process has been terminated abnormally, see %s for details." % self.bash_log_file.name)

        # get the output of shell command
        output = self.__run_command(sh_cmd)
        if is_trip_output_end_blanklines:
            for i in range(len(output)-1, 0, -1):
                if len(output[i].strip()) == 0:
                    del output[i]
                else:
                    break

        return output

    # execute global shell:
    # 1) set input stream -> $RAW_STR
    # 2) execute shell command from input
    # if error, write error message to err_log_file

    def exec_global_shell_with_orig_str(self, input, sh_cmd, is_trip_output_end_blanklines):
        self.v_cnt = 1 + self.v_cnt
        escape_in = input.replace('\'', "'\\''")
        # send shell cmd and set the temp RAW_STR
        cmd = ''' export RAW_STR%d='%s' && export RAW_STR=$RAW_STR%d && %s ; unset RAW_STR ''' % (
            self.v_cnt, escape_in, self.v_cnt, sh_cmd)
        return self.exec_global_shell(cmd, is_trip_output_end_blanklines)

    # extract shell, sql part from one line with format: @header '': SQL
    # return row: (found the header or not?, the extracted shell, the SQL in the left part)
    def extract_sh_cmd(self, header, input_str):
        start = len(header)
        is_start = False
        end = 0
        is_trip_comma = False
        res_cmd = ""
        res_sql = ""

        input_str = input_str.lstrip()
        if not input_str.startswith(header):
            return (False, None, None)

        for i in range(start, len(input_str)):
            if end == 0 and input_str[i] == '\'':
                if not is_start:
                    # find shell begin position
                    is_start = True
                    start = i+1
                    continue
                cnt = 0
                for j in range(i-1, 0, -1):
                    if input_str[j] == '\\':
                        cnt = 1 + cnt
                    else:
                        break
                if cnt % 2 == 1:
                    continue
                # find shell end position
                res_cmd = input_str[start: i]
                end = i
                continue
            if end != 0:
                # skip space until ':'
                if input_str[i] == ' ':
                    continue
                elif input_str[i] == ':':
                    is_trip_comma = True
                    res_sql = input_str[i+1:]
                    break
        if not is_start or end == 0 or not is_trip_comma:
            raise Exception("Invalid format: %s", input_str)
        #unescape \' to ' and \\ to '
        res_cmd = res_cmd.replace('\\\'', '\'')
        res_cmd = res_cmd.replace('\\\\', '\\')
        return (True, res_cmd, res_sql)

class SQLIsolationExecutor(object):
    def __init__(self, dbname=''):
        self.processes = {}
        # The re.S flag makes the "." in the regex match newlines.
        # When matched against a command in process_command(), all
        # lines in the command are matched and sent as SQL query.
        self.command_pattern = re.compile(r"^(-?\d+|[*])([&\\<\\>URSMq]*?)\:(.*)", re.S)
        if dbname:
            self.dbname = dbname
        else:
            self.dbname = os.environ.get('PGDATABASE')

    # To indicate the session has not been created or terminated.
    class SessionError(Exception):
        def __init__(self, name, mode, msg):
            super(SQLIsolationExecutor.SessionError, self).__init__(msg)
            self.name = name
            self.mode = mode

    class SQLConnection(object):
        def __init__(self, out_file, name, mode, dbname, user = None, passwd = None):
            self.name = name
            self.mode = mode
            self.out_file = out_file
            self.dbname = dbname
            self.user = user
            self.passwd = passwd

            parent_conn, child_conn = multiprocessing.Pipe(True)
            self.p = multiprocessing.Process(target=self.session_process, args=(child_conn,))
            self.pipe = parent_conn
            self.has_open = False
            self.p.start()

            # Close "our" copy of the child's handle, so that if the child dies,
            # recv() on the pipe will fail.
            child_conn.close()

            self.out_file = out_file

        def session_process(self, pipe):
            sp = SQLIsolationExecutor.SQLSessionProcess(self.name,
                self.mode, pipe, self.dbname, user=self.user, passwd=self.passwd)
            sp.do()

        def query(self, command, post_run_cmd = None, global_sh_executor = None):
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
            if re.match(r"^#.*:", r):
                raise SQLIsolationExecutor.SessionError(self.name, self.mode, r)

            if post_run_cmd != None:
                new_out = global_sh_executor.exec_global_shell_with_orig_str(r.rstrip(), post_run_cmd, True)
                for line in new_out:
                    print(line.rstrip(), file=self.out_file)
            else:
                print(r.rstrip(), file=self.out_file)

        def fork(self, command, blocking, global_sh_executor):
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
        def __init__(self, name, mode, pipe, dbname, user = None, passwd = None):
            """
                Constructor
            """
            self.name = name
            self.mode = mode
            self.pipe = pipe
            self.dbname = dbname
            self.passwd = passwd
            self.user = user
            # If there is an exception thrown when creating session, save it and send
            # it to pipe when we get the first execute_command call.
            self.create_exception = None
            if self.mode == "utility":
                (hostname, port) = self.get_hostname_port(name, 'p')
                self.con = self.connectdb(given_dbname=self.dbname,
                                          given_host=hostname,
                                          given_port=port,
                                          given_opt="-c gp_role=utility",
                                          given_user=user,
                                          given_passwd=passwd)
            elif self.mode == "standby":
                # Connect to standby even when its role is recorded
                # as mirror.  This is useful for scenarios where a
                # test needs to promote a standby without using
                # gpactivatestandby.
                (hostname, port) = self.get_hostname_port(name, 'm')
                self.con = self.connectdb(given_dbname=self.dbname,
                                          given_host=hostname,
                                          given_port=port,
                                          given_user=user,
                                          given_passwd=passwd)
            elif self.mode == "retrieve":
                (hostname, port) = ConnectionInfo.get_hostname_port(name, 'p')
                self.con = self.connectdb(given_dbname=self.dbname,
                                          given_host=hostname,
                                          given_port=port,
                                          given_opt="-c gp_retrieve_conn=true",
                                          given_user=user,
                                          given_passwd=passwd)
            elif self.mode == "mirror":
                # Connect to mirror even when it's role is recorded
                # as mirror.  This is useful for scenarios where a
                # primary is marked down but could actually accept
                # connection. This implies utility connection.
                (hostname, port) = self.get_hostname_port(name, 'm')
                self.con = self.connectdb(given_dbname=self.dbname,
                                          given_host=hostname,
                                          given_port=port,
                                          given_opt="-c gp_role=utility")
            else:
                self.con = self.connectdb(self.dbname)

        def connectdb(self, given_dbname, given_host = None, given_port = None, given_opt = None, given_user = None, given_passwd = None):
            con = None
            retry = 1000
            while retry:
                try:
                    if (given_port is None):
                        con = pg.connect(host= given_host,
                                          opt= given_opt,
                                          dbname= given_dbname,
                                          user = given_user,
                                          passwd = given_passwd)
                    else:
                        con = pg.connect(host= given_host,
                                                  port= given_port,
                                                  opt= given_opt,
                                                  dbname= given_dbname,
                                                  user = given_user,
                                                  passwd = given_passwd)
                    break
                except Exception as e:
                    if self.mode == "retrieve" and ("auth token is invalid" in str(e) or "Authentication failure" in str(e) or "does not exist" in str(e)):
                        self.create_exception = e
                        break
                    elif (("the database system is starting up" in str(e) or
                         "the database system is resetting" in str(e) or
                         "the database system is in recovery mode" in str(e)) and
                        retry > 1):
                        retry -= 1
                        time.sleep(0.1)
                    else:
                        raise
            if con is not None:
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
                if self.create_exception:
                    # When parent process received this, it should know the connection has not been
                    # created. Thus, the process entry should be cleared.
                    self.pipe.send("#%s%s> %s" % (self.name, self.mode, str(self.create_exception)))
                else:
                    r = self.execute_command(c)
                    self.pipe.send(r)
                r = None

                (c, wait) = self.pipe.recv()


    def get_process(self, out_file, name, mode="", dbname="", user=None, passwd=None):
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
            self.processes[(name, mode)] = SQLIsolationExecutor.SQLConnection(out_file, name, mode, dbname, user, passwd)
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
        result = con.query("SELECT content FROM gp_segment_configuration WHERE role = 'p' order by content").getresult()
        if len(result) == 0:
            raise Exception("Invalid gp_segment_configuration contents")
        return [int(content[0]) for content in result]

    def __preprocess_sql(self, name, pre_run_cmd, sql, global_sh_executor):
        if not pre_run_cmd:
            return sql

        (hostname, port) = ConnectionInfo.get_hostname_port(name, 'p')
        # Inject the current hostname and port to the shell.
        global_sh_executor.exec_global_shell("GP_HOSTNAME=%s" % hostname, True)
        global_sh_executor.exec_global_shell("GP_PORT=%s" % port, True)
        sqls = global_sh_executor.exec_global_shell_with_orig_str(sql, pre_run_cmd, True)
        if (len(sqls) != 1):
            raise Exception("Invalid shell command: %s" % "\n".join(sqls))

        return sqls[0]

    def __get_retrieve_user_token(self, name, global_sh_executor):
        (hostname, port) = ConnectionInfo.get_hostname_port(name, 'p')
        global_sh_executor.exec_global_shell("GP_HOSTNAME=%s" % hostname, True)
        global_sh_executor.exec_global_shell("GP_PORT=%s" % port, True)
        out = global_sh_executor.exec_global_shell("get_retrieve_token", True)
        if (len(out) > 0):
            token = out[0]
        out = global_sh_executor.exec_global_shell("echo ${RETRIEVE_USER}", True)
        if (len(out) > 0):
            user = out[0]
        return (user, token)

    def process_command(self, command, output_file, global_sh_executor):
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
        retrieve_token = None
        retrieve_user = None
        pre_run_cmd = None
        post_run_cmd = None
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
            elif flag and flag[0] == "R":
                con_mode = "retrieve"
            elif flag and flag[0] == "M":
                con_mode = "mirror"
            sql = m.groups()[2]
            sql = sql.lstrip()
            # If db_name is specified , it should be of the following syntax:
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
            else:
                (found_hd, pre_run_cmd, ex_sql) =  global_sh_executor.extract_sh_cmd('@pre_run', sql)
                if found_hd:
                    sql = ex_sql
                else:
                    (found_hd, post_run_cmd, ex_sql) = global_sh_executor.extract_sh_cmd('@post_run', sql)
                    if found_hd:
                        sql = ex_sql

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
                sql_new = self.__preprocess_sql(process_name, pre_run_cmd, sql.strip(), global_sh_executor)
                self.get_process(output_file, process_name, con_mode, dbname=dbname).query(sql_new, post_run_cmd, global_sh_executor)
        elif flag == "&":
            self.get_process(output_file, process_name, con_mode, dbname=dbname).fork(sql.strip(), True, global_sh_executor)
        elif flag == ">":
            self.get_process(output_file, process_name, con_mode, dbname=dbname).fork(sql.strip(), False, global_sh_executor)
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
                sql_new = self.__preprocess_sql(name, pre_run_cmd, sql.strip(), global_sh_executor)
                self.get_process(output_file, name, con_mode, dbname=dbname).query(sql_new, post_run_cmd, global_sh_executor)
        elif flag == "U&":
            sql_new = self.__preprocess_sql(process_name, pre_run_cmd, sql.strip(), global_sh_executor)
            self.get_process(output_file, process_name, con_mode, dbname=dbname).fork(sql_new, True, global_sh_executor)
        elif flag == "U<":
            if len(sql) > 0:
                raise Exception("No query should be given on join")
            self.get_process(output_file, process_name, con_mode, dbname=dbname).join()
        elif flag == "Uq":
            if len(sql) > 0:
                raise Exception("No query should be given on quit")
            self.quit_process(output_file, process_name, con_mode, dbname=dbname)
        elif flag == "S":
            sql_new = self.__preprocess_sql(process_name, pre_run_cmd, sql.strip(), global_sh_executor)
            self.get_process(output_file, process_name, con_mode, dbname=dbname).query(sql_new, post_run_cmd, global_sh_executor)
        elif flag == "R":
            if process_name == '*':
                process_names = [str(content) for content in self.get_all_primary_contentids(dbname)]
            else:
                process_names = [process_name]

            for name in process_names:
                try:
                    sql_new = self.__preprocess_sql(name, pre_run_cmd, sql.strip(), global_sh_executor)
                    (retrieve_user, retrieve_token) = self.__get_retrieve_user_token(name, global_sh_executor)
                    self.get_process(output_file, name, con_mode, dbname=dbname, user=retrieve_user, passwd=retrieve_token).query(sql_new, post_run_cmd, global_sh_executor)
                except SQLIsolationExecutor.SessionError as e:
                    print(str(e), file=output_file)
                    self.processes[(e.name, e.mode)].terminate()
                    del self.processes[(e.name, e.mode)]
        elif flag == "R&":
            sql_new = self.__preprocess_sql(process_name, pre_run_cmd, sql.strip(), global_sh_executor)
            (retrieve_user, retrieve_token) = self.__get_retrieve_user_token(process_name, global_sh_executor)
            self.get_process(output_file, process_name, con_mode, dbname=dbname, user=retrieve_user, passwd=retrieve_token).fork(sql_new, True, global_sh_executor)
        elif flag == "R<":
            if len(sql) > 0:
                raise Exception("No query should be given on join")
            (retrieve_user, retrieve_token) = self.__get_retrieve_user_token(process_name, global_sh_executor)
            self.get_process(output_file, process_name, con_mode, dbname=dbname, user=retrieve_user, passwd=retrieve_token).join()
        elif flag == "Rq":
            if len(sql) > 0:
                raise Exception("No query should be given on quit")

            if process_name == '*':
                process_names = [str(content) for content in self.get_all_primary_contentids(dbname)]
            else:
                process_names = [process_name]

            for name in process_names:
                try:
                    self.quit_process(output_file, name, con_mode, dbname=dbname)
                except Exception as e:
                    print(str(e), file=output_file)
                    pass
        elif flag == "M":
            self.get_process(output_file, process_name, con_mode, dbname=dbname).query(sql.strip(), post_run_cmd, global_sh_executor)
        elif flag == "Mq":
            if len(sql) > 0:
                raise Exception("No query should be given on quit Mq")
            self.quit_process(output_file, process_name, con_mode, dbname=dbname)
        else:
            raise Exception("Invalid isolation flag")

    def process_isolation_file(self, sql_file, output_file, initfile_prefix):
        """
            Processes the given sql file and writes the output
            to output file
        """
        shell_executor = GlobalShellExecutor(output_file, initfile_prefix)
        try:
            command = ""
            newline = False
            for line in sql_file:
                # this logic replicates the python2 behavior of a trailing comma at the end of print
                # i.e. ''' print >>output_file, line.strip(), '''
                print((" " if command and not newline else "") + line.strip(), end="", file=output_file)
                newline = False
                if line[0] == "!":
                    command_part = line # shell commands can use -- for long options like --include
                elif re.match(r";.*--", line) or re.match(r"^--", line):
                    command_part = line.partition("--")[0] # remove comment from line
                else:
                    command_part = line
                if command_part == "" or command_part == "\n":
                    print(file=output_file)
                    newline = True
                elif re.match(r".*;\s*$", command_part) or re.match(r"^\d+[q\\<]:\s*$", line) or re.match(r"^\*Rq:$", line) or re.match(r"^-?\d+[SUMR][q\\<]:\s*$", line):
                    command += command_part
                    try:
                        self.process_command(command, output_file, shell_executor)
                    except GlobalShellExecutor.ExecutionError as e:
                        # error of the GlobalShellExecutor cannot be recovered
                        raise
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
            shell_executor.terminate()
            raise
        finally:
            for process in list(self.processes.values()):
                process.terminate()
            shell_executor.terminate()

class SQLIsolationTestCase:
    """
        The isolation test case allows a fine grained control of interleaved
        executing transactions. This is mainly used to test isolation behavior.

        [<#>[flag]:] <sql> | ! <shell scripts or command>
        #: either an integer indicating a unique session, or a content-id if
           followed by U (for utility-mode connections) or R (for retrieve-mode
           connection). In 'U' mode or 'R' mode, the
           content-id can alternatively be an asterisk '*' to perform a
           utility-mode/retrieve-mode query on the master and all primary segments.
           If you want to create multiple connections to the same content-id, just
           increase N in: "content-id + {gpdb segment node number} * N",
           e.g. if gpdb cluster segment number is 3, then:
           (1) the master utility connections can be: -1U, -4U, -7U;
           (2) the seg0 connections can be: 0U, 3U, 6U;
           (3) the seg1 connections can be: 1U, 4U, 7U;
           (4) the seg2 connections can be: 2U, 5U, 8U;
        flag:
            &: expect blocking behavior
            >: running in background without blocking
            <: join an existing session
            q: quit the given session without blocking

            U: connect in utility mode to primary contentid from gp_segment_configuration
            U&: expect blocking behavior in utility mode (does not currently support an asterisk target)
            U<: join an existing utility mode session (does not currently support an asterisk target)

            R|R&|R<: similar to 'U' meaning except that the connect is in retrieve mode, here don't
               thinking about retrieve mode authentication, just using the normal authentication directly.

        An example is:

        Execute BEGIN in transaction 1
        Execute BEGIN in transaction 2
        Execute INSERT in transaction 2
        Execute SELECT in transaction 1
        Execute COMMIT in transaction 2
        Execute SELECT in transaction 1

        The isolation tests are specified identical to sql-scripts in normal
        SQLTestCases. However, it is possible to prefix a SQL line with
        an transaction identifier followed by a colon (":").
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
        Note: Do not expect blocking behavior from explicit quit statements.
        This implies that a subsequent statement can execute while the relevant
        session is still undergoing exit.

        Shell Execution for SQL or Output:

        @pre_run can be used for executing shell command to change input (i.e. each SQL statement) or get input info;
        @post_run can be used for executing shell command to change output (i.e. the result set printed for each SQL execution)
        or get output info. Just use the env variable ${RAW_STR} to refer to the input/out stream before shell execution,
        and the output of the shell command will be used as the SQL executed or output printed into results file.

        1: @post_run ' TOKEN1=` echo "${RAW_STR}" | awk \'NR==3\' | awk \'{print $1}\'` && export MATCHSUBS="${MATCHSUBS}${NL}m/${TOKEN1}/${NL}s/${TOKEN1}/token_id1/${NL}" && echo "${RAW_STR}" ': SELECT token,hostname,status FROM GP_ENDPOINTS WHERE cursorname='c1';
        2R: @pre_run ' echo "${RAW_STR}" | sed "s#@TOKEN1#${TOKEN1}#" ': RETRIEVE ALL FROM "@TOKEN1";

        These 2 sample is to:
        - Sample 1: set env variable ${TOKEN1} to the cell (row 3, col 1) of the result set, and print the raw result.
          The env var ${MATCHSUBS} is used to store the matchsubs section so that we can store it into initfile when
          this test case file is finished executing.
        - Sample 2: replace "@TOKEN1" by generated token which is fetched in sample1

        There are some helper functions which will be sourced automatically to make above
        cases easier. See global_sh_executor.sh for more information.
        $RETRIEVE_USER is a special environment vars which will be read by python to use them
        as the username for retrieve mode session. `None` will be used if the value has not
        been set when start retrieve mode session. See the get_retrieve_token in global_sh_executor.sh
        for more information about how to get the retrieve session password.

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
            executor.process_isolation_file(open(sql_file), f, out_file)
            f.flush()

        if out_file[-2:] == '.t':
            out_file = out_file[:-2]

        return out_file

if __name__ == "__main__":

    parser = OptionParser()
    parser.add_option("--dbname", dest="dbname",
                      help="connect to database DBNAME", metavar="DBNAME")
    parser.add_option("--initfile_prefix", dest="initfile_prefix",
                      help="The file path prefix for automatically generated initfile", metavar="INITFILE_PREFIX")
    (options, args) = parser.parse_args()

    # Explicitly set multiprocessing start method to 'fork' (Unix
    # default) to make isolation2 work with python3.8+ on MacOS.
    if sys.version_info >= (3, 8) and sys.platform == "darwin":
        multiprocessing.set_start_method('fork')

    executor = SQLIsolationExecutor(dbname=options.dbname)

    executor.process_isolation_file(sys.stdin, sys.stdout, options.initfile_prefix)
