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
    sql = """SELECT procpid FROM pg_stat_activity WHERE procpid != pg_backend_pid()"""
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

def get_master_filespace_map(conn):
    """Returns an array of [fsname, fspath] arrays that represents
    all the filespaces on the master."""
    sql = """SELECT fsname, fselocation 
    FROM pg_filespace, pg_filespace_entry 
WHERE pg_filespace.oid = pg_filespace_entry.fsefsoid 
    AND fsedbid = (SELECT dbid FROM gp_segment_configuration
                    WHERE role = 'p' AND content = -1)"""
    cursor = None
    try:
        cursor = dbconn.execSQL(conn, sql)
        return cursor.fetchall()
    finally:
        if cursor:
            cursor.close()


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


def vacuum_catalog(dburl,conn,full=False,utility=False):
    """ Will use the provided connection to enumerate the list of databases
        and then connect to each one in turn and vacuum full all of the 
        catalog files
        
        TODO:  There are a few tables that are cluster-wide that strictly speaking
               don't need to be vacuumed for each database.  These are most likely
               small and so perhaps isn't worth the added complexity to optimize them.
    
        WARNING:  doing a vacuum full on the catalog requires that 
        there aren't any users idle in a transaction as they typically
        hold catalog share locks.  The result is this vacuum will wait forever on
        getting the lock.  This method is best called when no one else
        is connected to the system.  Our own connections are typically idle
        in transactions and so are especially bad.
    """
    dblist = getDatabaseList(conn)
    catlist = get_catalogtable_list(conn)
    conn.commit()
    
    for db in dblist:
        test_url = copy.deepcopy(dburl)
        test_url.pgdb = db[0]
        
        if db[0] == 'template0' or db[0] == 'postgres':
            continue
        
        vac_conn = dbconn.connect(test_url,utility)
        vac_curs = vac_conn.cursor()
        vac_curs.execute("COMMIT")
        vac_curs.execute("SET CLIENT_MIN_MESSAGES='ERROR'")
        for table in catlist:
            logger.debug('Vacuuming %s %s' % (db[0],table[0]) )
            
            if full:
                sql = "VACUUM FULL %s" % table[0]
            else:
                sql = "VACUUM %s" % table[0]
            
            vac_curs.execute(sql)
            
        
        vac_curs.execute(sql)
        vac_conn.commit()
        vac_conn.close()
