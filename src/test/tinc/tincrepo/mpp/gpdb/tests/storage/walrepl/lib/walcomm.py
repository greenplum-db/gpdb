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
import select

from mpp.gpdb.tests.storage.walrepl.lib.pqwrap import *


# access/xlog.h
class XLogRecPtr(Structure):
    _fields_ = [("xlogid", c_uint32),
                 ("xrecoff", c_uint32)]

    @staticmethod
    def from_string(lsn):
        (logid, recoff) = lsn.split('/')
        return XLogRecPtr(int(logid, 16), int(recoff, 16))

    def __str__(self):
        return '{0:X}/{1:X}'.format(self.xlogid, self.xrecoff)

# replication/walprotocol.h
class StandbyReplyMessage(Structure):
    _fields_ = [("write", XLogRecPtr),
                ("flush", XLogRecPtr),
                ("apply", XLogRecPtr),
                ("sendTime", c_int64)]

# replication/walprotocol.h
class WalDataMessageHeader(Structure):
    _fields_ = [("dataStart", XLogRecPtr),
                ("walEnd", XLogRecPtr),
                ("sendTime", c_int64)]

class WalMessage(object):
    """
    The base class of various message.  See receive function below.
    """
    msgtype = None
    msglen = 0
    errmsg = None

class WalMessageData(WalMessage):
    def __init__(self, msgbuf, msglen):
        header = WalDataMessageHeader()
        # memmove is required for alignment reasons
        memmove(addressof(header), msgbuf, sizeof(WalDataMessageHeader))
        self.header = header
        self.msgtype = 'w'
        self.msglen = msglen

class WalMessageNoData(WalMessage):
    def __init__(self, is_timeout=False):
        self.is_timeout = is_timeout

class WalMessageExit(WalMessage):
    def __init__(self, errmsg):
        self.errmsg = errmsg

class WalMessageError(WalMessage):
    def __init__(self, errmsg):
        self.errmsg = errmsg

class WalClient(PGconn):
    """
    This is the main class to access walsender.
    A normal usage is shown in the bottom of this file.
    """

    def identify_system(self):
        """
        Send IDENTIFY_SYSTEM message.  Returns a tuple of system identifier,
        timeline ID, and XLOG start LSN.

        """
        return identify_system(self.conn)

    def start_replication(self, startpoint, sync=True):
        """
        Send START_REPLICATION message.  startpoint is an LSN to start
        streaming.

        """
        return start_replication(self.conn, startpoint, sync)

    def reply(self, write_point, flush_point, apply_point):
        """
        Send a reply message.  The message will keep the server
        updating and prevents it from disconnecting.

        """
        return reply(self.conn, write_point, flush_point, apply_point)

    def receive(self, timeout):
        """
        Wait for the server message, or timeout.  Returns any of
        WalMessageData, WalMessageNoData, WalMessageExit or WalMessageError.
        """

        return receive(self.conn, timeout)

def identify_system(conn):
    res = libpq.PQexec(conn, "IDENTIFY_SYSTEM")
    if libpq.PQresultStatus(res) != PGRES_TUPLES_OK:
        raise StandardError(libpq.PQerrorMessage(conn))

    if libpq.PQnfields(res) != 3 or libpq.PQntuples(res) != 1:
        raise StandardError("3 fields / 1 tuple is expected, "
                            "got {nfield} fields / {ntuple} tuples".format(
                                nfield=libpq.PQnfields(res),
                                ntuple=libpq.PQntuples(res)))
    sysid = libpq.PQgetvalue(res, 0, 0)
    tli = libpq.PQgetvalue(res, 0, 1)
    xpos = libpq.PQgetvalue(res, 0, 2)

    libpq.PQclear(res)
    return (sysid, tli, xpos)

def start_replication(conn, startpoint, sync=True):
    cmd = "START_REPLICATION {0:X}/{1:X}".format(
            startpoint.xlogid, startpoint.xrecoff)
    if sync:
        cmd = cmd + " SYNC"
    res = libpq.PQexec(conn, cmd)
    if libpq.PQresultStatus(res) != PGRES_COPY_BOTH:
        raise StandardError("could not start replication: {0}".format(
            libpq.PQerrorMessage(conn)))

    libpq.PQclear(res)

def reply(conn, write_point, flush_point, apply_point):
    buflen = 1 + sizeof(StandbyReplyMessage)
    buf = (c_char * buflen)()
    buf[0] = 'r'
    reply = StandbyReplyMessage(write_point, flush_point, apply_point, 0)
    memmove(addressof(buf) + 1, addressof(reply),
            sizeof(StandbyReplyMessage))
    if (libpq.PQputCopyData(conn, buf, buflen) <= 0 or
            libpq.PQflush(conn)):
        raise StandardError("could not send data to WAL stream: {0}".
                format(libpq.PQerrorMessage(conn)))

def receive(conn, timeout):
    recvBuf = c_void_p(None)  # use void_p for binary data
    # Try to receive a CopyData message
    rawlen = libpq.PQgetCopyData(conn, byref(recvBuf), 1)
    if rawlen == 0:
        # No data available yet. If the caller requested to block, wait for
        # more data to arrive.
        if timeout > 0:
            rlist = [libpq.PQsocket(conn)]
            (rout, _, _) = select.select(rlist, [], [], timeout / 1000.0)
            if len(rout) == 0:
                # timeout
                return WalMessageNoData(is_timeout=True)

        if libpq.PQconsumeInput(conn) == 0:
            errmsg = "could not receive data from WAL stream"
            return WalMessageError(errmsg)

        # Now that we've consumed some input, try again
        rawlen = libpq.PQgetCopyData(conn, byref(recvBuf), 1)
        if rawlen == 0:
            return WalMessageNoData(is_timeout=False)

    if rawlen == -1:
        # end-of-streaming or error
        res = libpq.PQgetResult(conn)
        if libpq.PQresultStatus(res) == PGRES_COMMAND_OK:
            errmsg = "replication terminated by primary server"
        else:
            errmsg = "could not receive data from WAL stream"
        libpq.PQclear(res)

        return WalMessageExit(errmsg)

    if rawlen < -1:
        errmsg = "could not receive data from WAL stream"
        return WalMessageError(errmmsg)

    # c_void_p.value will return the pointer address
    # casting to POINTER(c_char) gives c_char array addressing.
    char_array = cast(recvBuf.value, POINTER(c_char))
    msgtype = char_array[0]
    msgbuf = c_void_p(recvBuf.value + sizeof(c_char))
    msglen = rawlen - sizeof(c_char)
    return WalMessageData(msgbuf, msglen)



if __name__ == '__main__':
    NAPTIME_PER_CYCLE = 3 * 1000

    with WalClient("dbname=replication replication=true") as client:
        if client.status() != CONNECTION_OK:
            raise StandardError(client.error_message())

        (sysid, tli, xpos) = client.identify_system()
        print sysid, tli, xpos

        xpos_ptr = XLogRecPtr.from_string(xpos)
        client.start_replication(xpos_ptr)

        while True:
            msg = client.receive(NAPTIME_PER_CYCLE)
            if isinstance(msg, WalMessageData):
                header = msg.header
                print "msglen = {0}, dataStart = {1}, walEnd = {2}".format(
                        msg.msglen, header.dataStart, header.walEnd)
                # xpos_ptr is yet dummy
                client.reply(xpos_ptr, xpos_ptr, xpos_ptr)
            elif isinstance(msg, WalMessageNoData):
                print "timeout, replying..."
                client.reply(xpos_ptr, xpos_ptr, xpos_ptr)
            elif isinstance(msg, WalMessageExit):
                print msg.errmsg
                break
            else:
                raise StandardError(msg.errmsg)
