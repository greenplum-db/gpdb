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

from ctypes import *
import os

# load from installation.  The library lookup in ctypes is a bit strange,
# so make sure you are loading correct one.
libpqdir = '%s/lib' % (os.environ.get('GPHOME'))
try:
    libpq = CDLL(os.path.join(libpqdir, 'libpq.dylib'))
except:
    libpq = CDLL(os.path.join(libpqdir, 'libpq.so'))


# See src/interfaces/libpq/libpq-fe.h for more details.
CONNECTION_OK = 0
CONNECTION_BAD = 1
CONNECTION_STARTED = 2
CONNECTION_MADE = 3
CONNECTION_AWAITING_RESPONSE = 4
CONNECTION_AUTH_OK = 5
CONNECTION_SETENV = 6
CONNECTION_SSL_STARTUP = 7
CONNECTION_NEEDED = 8

PGRES_EMPTY_QUERY = 0
PGRES_COMMAND_OK = 1
PGRES_TUPLES_OK = 2
PGRES_COPY_OUT = 3
PGRES_COPY_IN = 4
PGRES_BAD_RESPONSE = 5
PGRES_NONFATAL_ERROR = 6
PGRES_FATAL_ERROR = 7
PGRES_COPY_BOTH = 8
PGRES_SINGLE_TUPLE = 9

libpq.PQerrorMessage.restype = c_char_p
libpq.PQresStatus.restype = c_char_p
libpq.PQresultErrorMessage.restype = c_char_p
libpq.PQfname.restype = c_char_p
libpq.PQgetvalue.restype = c_char_p

# src/include/catalog/pg_type.h
BOOLOID = 16
CHAROID = 18
NAMEOID = 19
INT8OID = 20
INT2OID = 21
INT4OID = 23
TEXTOID = 25
OIDOID = 26
FLOAT4OID = 700
FLOAT8OID = 701
BPCHAROID = 1042
VARCHAROID = 1043
DATEOID = 1082
TIMEOID = 1083
TIMESTAMPOID = 1114
TIMESTAMPTZOID = 1184

NoticeReceiverFunc = CFUNCTYPE(None, c_void_p, c_void_p)

class PGconn(object):

    def __init__(self, conninfo):
        if type(conninfo) == str:
            self.conninfo = conninfo
            self.conn = libpq.PQconnectdb(conninfo)
        else:
            # otherwise, assume it's a PGconn object
            self.conn = conninfo

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.finish()

    def status(self):
        return libpq.PQstatus(self.conn)

    def error_message(self):
        return libpq.PQerrorMessage(self.conn)

    def finish(self):
        if self.conn:
            libpq.PQfinish(self.conn)
            self.conn = None

    def execute(self, query):
        res = libpq.PQexec(self.conn, query)
        return PGresult(res)

    # Async supports
    def send_query(self, query):
        return libpq.PQsendQuery(self.conn, query)

    def consume_input(self):
        return libpq.PQconsumeInput(self.conn)

    def is_busy(self):
        return libpq.PQisBusy(self.conn)

    def get_result(self):
        res = libpq.PQgetResult(self.conn)
        return PGresult(res)

    def set_notice_receiver(self, proc, arg):
        return libpq.PQsetNoticeReceiver(self.conn, proc, arg)

    def fileno(self):
        # for select call
        return libpq.PQsocket(self.conn)

class PGresult(object):

    class Tuple(object):
        pass

    def __init__(self, res):
        self.res = res

    def status(self):
        return libpq.PQresultStatus(self.res)

    def error_message(self):
        return libpq.PQresultErrorMessage(self.res)

    def clear(self):
        if self.res:
            libpq.PQclear(self.res)
            self.res = None

    def ntuples(self):
        return libpq.PQntuples(self.res)

    def nfields(self):
        return libpq.PQnfields(self.res)

    def fname(self, col):
        return str(libpq.PQfname(self.res, col))

    def getvalue(self, row, col, convert=False):
        """Returns value at the position of row and column.
        If convert parameter is set to be True, the value will be
        converted to appropriate python object based on type oid,
        otherwise all values are python string.
        SQL NULL value will be returned as None in any case.
        """

        if libpq.PQgetisnull(self.res, row, col) == 1:
            return None
        val = libpq.PQgetvalue(self.res, row, col)
        if convert:
            valtype = libpq.PQftype(self.res, col)
            return convert_type(val, valtype)
        else:
            return str(val)

    def getpyvalue(self, row, col):
        """Returns value as python object mapped by type oid"""

        return self.getvalue(row, col, convert=True)

    def tuples(self, to_obj=False, convert=False):
        """Returns result set in either list of list or object, if to_obj
        parameter is True, which has attributes of column name with the
        value.  The values are converted to python objects if convert
        parameter is True.
        """

        result = list()
        for row in range(self.ntuples()):
            if to_obj:
                tup = PGresult.Tuple()
            else:
                tup = list()
            for col in xrange(self.nfields()):
                val = self.getvalue(row, col, convert)
                if to_obj:
                    setattr(tup, self.fname(col), val)
                else:
                    tup.append(val)
            result.append(tup)
        return result

    def pytuples(self):
        """Returns result set as a list of list."""

        return self.tuples(convert=True)

    def objects(self):
        """Returns result set as a list of object, without conversion."""

        return self.tuples(to_obj=True)

    def pyobjects(self):
        """Returns result set as a list of object, with each value
        mapped to python object.
        """

        return self.tuples(to_obj=True, convert=True)

def convert_type(cstr, typid):
    if typid == BOOLOID:
        return bool(cstr == 't')
    elif (typid == INT8OID or typid == INT2OID or typid == INT4OID or
        typid == OIDOID):
        return int(cstr)
    elif typid == FLOAT4OID or typid == FLOAT8OID:
        return float(cstr)

    return str(cstr)
