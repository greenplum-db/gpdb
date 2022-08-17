#!/usr/bin/env python3
#-*- coding: utf-8 -*-

# This is just to test we can use dbconn to connect a database
# whose name contains special chars. This test needs to have
# a cluster running so seems not easy under unittest. Previously,
# I try to create a UDF to test it udner regress test, however,
# import dbconn, its dependency package will import sys and access
# argv, which is not allowed in plpython. So finally, I create
# the python script in this directory, and use \! to run it and
# test in regress/dispatch and regress/gpcopy.

import sys
from gppylib.db import dbconn

dbnames = ['funny\"db\'with\\\\quotes',    # from regress/dispatch
           'funny copy\"db\'with\\\\quotes'# from regress/gpcopy
]

def test_connect_special_dbname(dbname):
    url = dbconn.DbURL(dbname=dbname)
    conn = dbconn.connect(url)
    count = dbconn.querySingleton(conn, "select 1")
    result = (count == 1)
    conn.close()


if __name__ == "__main__":
    dbname = dbnames[int(sys.argv[1])]
    test_connect_special_dbname(dbname)
