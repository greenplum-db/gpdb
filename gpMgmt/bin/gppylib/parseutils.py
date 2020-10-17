#!/usr/bin/env python3
# Line too long            - pylint: disable=C0301
# Invalid name             - pylint: disable=C0103

"""
  parseutils.py
    
  Routines to parse "flexible" configuration files for tools like 
    gpaddmirrors, gprecoverseg, gpexpand, etc.

  Copyright (c) EMC/Greenplum Inc 2011. All Rights Reserved. 
"""

import re
import socket
from gppylib.mainUtils import ExceptionNoStackTraceNeeded

# NOTE: the parser here leaves each field as is; it only checks the number
#  and type of the arguments.  In particular, any address/hostname with '[]` is
#  passed back unmodified.
def canonicalize_address(addr):
    """
    Encases addresses in [ ] per RFC 2732.  Generally used to deal with ':'
    characters which are also often used as delimiters.

    Returns the addr string if it doesn't contain any ':' characters.

    If addr contains ':' and also contains a '[' then the addr string is
    simply returned under the assumption that it is already escaped as needed.

    Otherwise return a new string from addr by adding '[' prefix and ']' suffix.

    Examples
    --------

    >>> canonicalize_address('myhost')
    'myhost'
    >>> canonicalize_address('127.0.0.1')
    '127.0.0.1'
    >>> canonicalize_address('::1')
    '[::1]'
    >>> canonicalize_address('[::1]')
    '[::1]'
    >>> canonicalize_address('2620:0:170:610::13')
    '[2620:0:170:610::13]'
    >>> canonicalize_address('[2620:0:170:610::13]')
    '[2620:0:170:610::13]'

    @param addr: the address to possibly encase in [ ]
    @returns:    the address, encased in [] if necessary
    """
    if ':' not in addr: return addr
    if '[' in addr: return addr
    return '[' + addr + ']'

# NOTE: these checks are not strict, but are sanity checks
#  The caller is responsible for validating the parsed results.


def _is_valid_base10_int(arg, lower_limit):
    try:
        port = int(arg)
        return port >= lower_limit
    except ValueError:
        return False


def is_valid_contentid(arg):
    return _is_valid_base10_int(arg, -1)


def is_valid_dbid(arg):
    return _is_valid_base10_int(arg, 1)


def is_valid_port(arg):
    return _is_valid_base10_int(arg, 0)


def is_valid_address(address):
    return False if not address else True


def is_valid_role(role):
    return role in ['p', 'm']


def is_valid_datadir(datadir):
    return False if not datadir else True


def check_values(lineno, address=None, port=None, datadir=None, content=None, hostname=None, dbid=None, role=None):
    if address is not None and not is_valid_address(address):
        raise ExceptionNoStackTraceNeeded("Invalid address on line %d" % lineno)
    if port is not None and not is_valid_port(port):
        raise ExceptionNoStackTraceNeeded("Invalid port on line %d" % lineno)
    if datadir is not None and not is_valid_datadir(datadir):
        raise ExceptionNoStackTraceNeeded("Invalid directory on line %d" % lineno)
    if content is not None and not is_valid_contentid(content):
        raise ExceptionNoStackTraceNeeded("Invalid content ID on line %d" % lineno)
    if hostname is not None and not is_valid_address(hostname):
        raise ExceptionNoStackTraceNeeded("Invalid hostname on line %d" % lineno)
    if dbid is not None and not is_valid_dbid(dbid):
        raise ExceptionNoStackTraceNeeded("Invalid dbid on line %d" % lineno)
    if role is not None and not is_valid_role(role):
        raise ExceptionNoStackTraceNeeded("Invalid role on line %d" % lineno)


def line_reader(f):
    """
    Read the contents of the given input, generating the non-blank non-comment
    lines found within as a series of tuples of the form (line number, line).

    >>> [l for l in line_reader(['', '# test', 'abc:def'])]
    [(3, 'abc:def')]

    """
    for offset, line in enumerate(f):
        line = line.strip()
        if len(line) < 1 or line[0] == '#':
            continue
        yield offset+1, line


if __name__ == '__main__':
    import doctest
    doctest.testmod()
