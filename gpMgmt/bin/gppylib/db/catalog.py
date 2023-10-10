#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#
""" Provides Access Utilities for Examining and Modifying the GP Catalog.

"""
import copy
import os
from contextlib import closing
from gppylib import gplog
from gppylib.db import dbconn

logger=gplog.get_default_logger()


class RemoteQueryCommand:
    def __init__(self, qname, query, hostname, port, dbname=None):
        self.qname = qname
        self.query = query
        self.hostname = hostname
        self.port = port
        self.dbname = dbname or os.environ.get('PGDATABASE', None) or 'template1'
        self.res = None

    def get_results(self):
        return self.res

    def run(self):
        logger.debug('Executing query (%s:%s) for segment (%s:%s) on database (%s)' % (
            self.qname, self.query, self.hostname, self.port, self.dbname))
        with closing(dbconn.connect(dbconn.DbURL(hostname=self.hostname, port=self.port, dbname=self.dbname),
                                    utility=True)) as conn:
            self.res = dbconn.query(conn, self.query).fetchall()


class CatalogError(Exception): pass

def basicSQLExec(conn,sql):
    cursor=None
    try:
        cursor = dbconn.query(conn,sql)
        rows=cursor.fetchall()
        return rows
    finally:
        if cursor:
            cursor.close()

def getSessionGUC(conn,gucname):
    sql = "SHOW %s" % gucname
    return basicSQLExec(conn,sql)[0][0]

def getUserDatabaseList(conn):
    sql = "SELECT datname FROM pg_catalog.pg_database WHERE datname NOT IN ('postgres','template1','template0') ORDER BY 1"
    return basicSQLExec(conn,sql)

def getDatabaseList(conn):
    sql = "SELECT datname FROM pg_catalog.pg_database"
    return basicSQLExec(conn,sql)

def getUserConnectionInfo(conn):
    """dont count ourselves"""
    header = ["pid", "usename", "application_name", "client_addr", "client_hostname", "client_port", "backend_start", "query"]
    sql = """SELECT pid, usename, application_name, client_addr, client_hostname, client_port, backend_start, query FROM pg_stat_activity WHERE pid != pg_backend_pid() and backend_type = 'client backend' ORDER BY usename"""
    return header, basicSQLExec(conn,sql)

def doesSchemaExist(conn,schemaname):
    sql = "SELECT nspname FROM pg_catalog.pg_namespace WHERE nspname = '%s'" % schemaname
    cursor=None
    try:
        cursor = dbconn.query(conn,sql)
        numrows = cursor.rowcount
        if numrows == 0:
            return False
        elif numrows == 1:
            return True
        else:
            raise CatalogError("more than one entry in pg_namespace for '%s'" % schemaname)
    finally:
        if cursor: 
            cursor.close()

def dropSchemaIfExist(conn,schemaname):
    sql = "SELECT nspname FROM pg_catalog.pg_namespace WHERE nspname = '%s'" % schemaname
    dropsql = "DROP SCHEMA %s" % schemaname
    cursor=None
    try:
        cursor = dbconn.query(conn,sql)
        numrows = cursor.rowcount
        if numrows == 1:
            cursor = dbconn.query(conn,dropsql)
        else:
            raise CatalogError("more than one entry in pg_namespace for '%s'" % schemaname)
    except Exception as e:
        raise CatalogError("error dropping schema %s: %s" % (schemaname, str(e)))
    finally:
        if cursor:
            cursor.close()
        conn.commit()
