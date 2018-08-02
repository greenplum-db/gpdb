#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#
""" Provides Access Utilities for Examining and Modifying the GP Catalog.

"""
import copy

import dbconn
from  gppylib import gplog

logger=gplog.get_default_logger()

class CatalogError(Exception): pass

def basicSQLExec(conn,sql):
    cursor=None
    try:
        cursor=dbconn.execSQL(conn,sql)
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

def getUserPIDs(conn):
    """dont count ourselves"""
    sql = """SELECT pid FROM pg_stat_activity WHERE pid != pg_backend_pid()"""
    return basicSQLExec(conn,sql)

def doesSchemaExist(conn,schemaname):
    sql = "SELECT nspname FROM pg_catalog.pg_namespace WHERE nspname = '%s'" % schemaname
    cursor=None
    try:
        cursor=dbconn.execSQL(conn,sql)
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
        cursor=dbconn.execSQL(conn,sql)
        numrows = cursor.rowcount
        if numrows == 1:
            cursor=dbconn.execSQL(conn,dropsql)
        else:
            raise CatalogError("more than one entry in pg_namespace for '%s'" % schemaname)
    except Exception as e:
        raise CatalogError("error dropping schema %s: %s" % (schemaname, str(e)))
    finally:
        if cursor:
            cursor.close()
        conn.commit()

def get_catalogtable_list(conn):
    sql = """SELECT schemaname || '.' ||  tablename 
             FROM pg_tables 
             WHERE schemaname = 'pg_catalog'
          """
    cursor=None
    try:
        cursor=dbconn.execSQL(conn,sql)
        
        return cursor.fetchall()
    finally:
        if cursor:
            cursor.close()
