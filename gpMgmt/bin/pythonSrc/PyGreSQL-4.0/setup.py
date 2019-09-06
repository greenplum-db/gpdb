#!/usr/bin/env python
# $Id: setup.py,v 1.27 2008/11/21 17:08:17 cito Exp $

"""Setup script for PyGreSQL version 4.0

Authors and history:
* PyGreSQL written 1997 by D'Arcy J.M. Cain <darcy@druid.net>
* based on code written 1995 by Pascal Andre <andre@chimay.via.ecp.fr>
* setup script created 2000/04 Mark Alexander <mwa@gate.net>
* tweaked 2000/05 Jeremy Hylton <jeremy@cnri.reston.va.us>
* win32 support 2001/01 by Gerhard Haering <gerhard@bigfoot.de>
* tweaked 2006/02 and 2008/11 by Christoph Zwerschke <cito@online.de>

Prerequisites to be installed:
* Python including devel package (header files and distutils)
* PostgreSQL libs and devel packages (header files of client and server)
* PostgreSQL pg_config tool (usually included in the devel package)
  (the Windows installer has it as part of the database server feature)

Tested with Python 2.5.2 and PostGreSQL 8.3.5. Older version should work
as well, but you will need at least Python 2.3 and PostgreSQL 7.4.

Use as follows:
python setup.py build   # to build the module
python setup.py install # to install it

You should use MinGW (www.mingw.org) for building on Win32:
python setup.py build -c mingw32 install # use MinGW
Note that Python newer than version 2.3 is using msvcr71 instead of msvcrt
as its common runtime library. So, if you are using MinGW to build PyGreSQL,
you should edit the file "%MinGWpath%/lib/gcc/%MinGWversion%/specs"
and change the entry that reads -lmsvcrt to -lmsvcr71.

See docs.python.org/doc/install/ for more information on
using distutils to install Python programs.

"""

version = "4.0"

import sys
import os

if not (2, 2) < sys.version_info[:2] < (3, 0):
    raise Exception("PyGreSQL %s requires a Python 2 version"
        " newer than 2.2." % version)

import os
from distutils.core import setup
from distutils.extension import Extension

def pg_config(s):
    """Retrieve information about installed version of PostgreSQL."""
    if os.path.exists("../../../../src/bin/pg_config/pg_config"):
        f = os.popen("../../../../src/bin/pg_config/pg_config --%s" % s)
    else:
        """If a VPATH build, it might not be there.  Look other places"""
        """It should be the one in the path, because the makefile includes greenplum_path.sh """
        f = os.popen("pg_config --%s" % s)

    d = f.readline().strip()
    if f.close() is not None:
        raise Exception("pg_config tool is not available.")
    if not d:
        raise Exception("Could not get %s information." % s)
    return os.getenv('DESTDIR','')+d

def mk_include():
    """Create a temporary local include directory.

    The directory will contain a copy of the PostgreSQL server header files,
    where all features which are not necessary for PyGreSQL are disabled.

    """
    os.mkdir('include')
    for f in os.listdir(pg_include_dir_server):
        if not f.endswith('.h'):
            continue
        d = open(os.path.join(pg_include_dir_server, f)).read()
        if f == 'pg_config.h':
            d += '\n'.join(('',
                '#undef ENABLE_NLS',
                '#undef USE_REPL_SNPRINTF',
                '#undef USE_SSL',
                '#undef USE_ZLIB',
                '#undef HAVE_STDINT_H',
                '#undef HAVE_SYS_TIME_H',
                '#undef HAVE_UNISTD_H',
                '#define _CRT_SECURE_NO_WARNINGS 1',
                '#define _USE_32BIT_TIME_T 1',
                ''))
        open(os.path.join('include', f), 'w').write(d)

def rm_include():
    """Remove the temporary local include directory."""
    if os.path.exists('include'):
        for f in os.listdir('include'):
            os.remove(os.path.join('include', f))
        os.rmdir('include')

pg_include_dir = pg_config('includedir')
pg_include_dir_server = pg_config('includedir-server')

rm_include()
mk_include()

include_dirs = ['include', pg_include_dir,  pg_include_dir_server]

pg_libdir = pg_config('libdir')
library_dirs = [pg_libdir]

libraries = ['pq']
extra_compile_args = ['-O2']

if sys.platform == "win32":
    include_dirs.append(os.path.join(pg_include_dir_server, 'port/win32'))
elif sys.platform == 'darwin' and sys.maxsize > 2**32:
    extra_compile_args.extend(['-arch', 'x86_64'])

setup(
    name="PyGreSQL",
    version=version,
    description="Python PostgreSQL Interfaces",
    long_description = ("PyGreSQL is an open-source Python module"
        " that interfaces to a PostgreSQL database."
        " It embeds the PostgreSQL query library to allow easy use"
        " of the powerful PostgreSQL features from a Python script."),
    keywords="postgresql database api dbapi",
    author="D'Arcy J. M. Cain",
    author_email="darcy@PyGreSQL.org",
    url="http://www.pygresql.org",
    download_url = "ftp://ftp.pygresql.org/pub/distrib/",
    platforms = ["any"],
    license="Python",
    py_modules=['pg', 'pgdb'],
    ext_modules=[Extension(
        '_pg', ['pgmodule.c'],
        include_dirs = include_dirs,
        library_dirs = library_dirs,
        libraries = libraries,
        extra_compile_args = extra_compile_args
    )],
    classifiers=[
        "Development Status :: 6 - Mature",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Python Software Foundation License",
        "Operating System :: OS Independent",
        "Programming Language :: C",
        "Programming Language :: Python",
        "Topic :: Database",
        "Topic :: Database :: Front-Ends",
        "Topic :: Software Development :: Libraries :: Python Modules"
    ]
)

rm_include()
