#!/usr/bin/env python
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

Global Variables that's commonly used by Automation Framework
Do not import any libraries other than Python Library.

Usage:
from lib.Globals import *

GPHOME - GPDB Home Directory
MASTER_DATA_DIRECTORY - GPDB Master Data Directory
PGPORT - Master GPDB Port



@note: Please add Global accordingly and add changelist below

@change: Full Name
"""

import os
import sys
import socket

"""
Python Environments
"""
GPTEST_LIBPATH = os.path.abspath(os.path.dirname(__file__))
mklibpath = lambda *x: os.path.join(GPTEST_LIBPATH, *x)
GPTEST_PATH = os.path.abspath(mklibpath(".."))
GPDATA_PATH = GPTEST_PATH + os.sep + "test_data"
GPDOC_PATH  = GPTEST_PATH + os.sep + "test_docs"
GPTEST_UNIT = GPTEST_PATH + os.sep + "test_libs"
QAUTILS_PATH = GPTEST_PATH + os.sep + "QAUtilities"

"""
Framework Global Values
"""
DBNAME = "gptest"

"""
Unix Environments
"""
USER = os.environ.get( "LOGNAME" )
HOST = socket.gethostname()

"""
GPDB Environments
"""
GPHOME = os.getenv("GPHOME")
GPLIB = GPHOME + os.sep + "lib"
GPLIB_POSTGRES = GPLIB + os.sep + "postgresql"
GPLIB_PYTHON = GPLIB + os.sep + "python"
MASTER_DATA_DIRECTORY = os.getenv("MASTER_DATA_DIRECTORY")

"""
Postgres Environments
http://www.postgresql.org/docs/current/static/libpq-envars.html
@warning: Most of these are not set by default.
"""
PGPORT = os.environ.get("PGPORT")
if PGPORT is None:
    PGPORT = 5432
PGUSER = os.environ.get("PGUSER")
if PGUSER is None:
    PGUSER = USER
PGHOST = os.environ.get("PGHOST")
if PGHOST is None:
    PGHOST = HOST
PGHBA_CONF = "%s/pg_hba.conf" % MASTER_DATA_DIRECTORY
PGDATABASE = os.environ.get("PGDATABASE")
if PGDATABASE is None:
    PGDATABASE = DBNAME
    
PGHOSTADDR = os.environ.get("PGHOSTADDR")
PGPASSWORD = os.environ.get("PGPASSWORD")
PGPASSFILE = os.environ.get("PGPASSFILE")
if PGPASSFILE is None:
    PGPASSFILE = os.getenv("HOME") + os.sep + ".pgpass"
PGAPPNAME = os.environ.get("PGAPPNAME")

