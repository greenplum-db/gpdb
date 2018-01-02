#!/usr/bin/env python
# Line too long            - pylint: disable=C0301
# Invalid name             - pylint: disable=C0103

"""
  parseutils.py
    
  Routines to parse "flexible" configuration files for tools like 
    gpaddmirrors, gprecoverseg, gpexpand, etc.

  Copyright (c) EMC/Greenplum Inc 2011. All Rights Reserved. 
"""

import sys
from gppylib.mainUtils import ExceptionNoStackTraceNeeded
from gppylib.gplog import get_default_logger, logging_is_verbose

logger = get_default_logger()

def caller():
    "Return name of calling function"
    if logging_is_verbose():
        return sys._getframe(1).f_code.co_name + '()'
    return ''


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
    @returns:    the addresss, encased in [] if necessary
    """
    if ':' not in addr: return addr
    if '[' in addr: return addr
    return '[' + addr + ']'


#
# line parsing
#

def consume_to(delimiter, rest):
    """
    Consume characters from rest string until we encounter the delimiter.
    Returns (None, after, None) where after are the characters after delimiter
    or (None, rest, 'does not contain '+delimiter) when delimiter is not encountered.
    
    Examples
    --------
    
    >>> consume_to('=', 'abc=def:ghi')
    (None, 'def:ghi', None)

    @param delimiter: the delimiter string
    @param rest:      the string to read such as 'abc:def:ghi'
    @returns:         (None, after, None) tuple such as (None, 'def:ghi', None)
    """
    p = rest.find(delimiter)
    if p < 0:
        return None, rest, 'does not contain '+delimiter
    return None, rest[p+1:], None



def read_to(delimiter, rest):
    """
    Read characters from rest string until we encounter the delimiter.
    Separate the string into characters 'before' and 'after' the delimiter.
    If no delimiter is found, assign entire string to 'before' and None to 'after'.

    Examples
    --------
    
    >>> read_to(':', 'abc:def:ghi')
    ('abc', 'def:ghi', None)
    >>> read_to(':', 'abc:def')
    ('abc', 'def', None)
    >>> read_to(':', 'abc')
    ('abc', None, None)
    >>> read_to(':', '')
    ('', None, None)

    Note this returns a 3-tuple for compatibility with other routines
    which use the third element as an error message

    @param delimiter: the delimiter string
    @param rest:      the string to read such as 'abc:def:ghi'
    @returns:         (before, after, None) tuple such as ('abc', 'def:ghi', None)
    """
    p = rest.find(delimiter)
    if p < 0:
        return rest, None, None
    return rest[0:p], rest[p+1:], None


def read_to_bracketed(delimiter, rest):
    """
    Read characters from rest string which is expected to start with a '['.
    If rest does not start with '[', return a tuple (None, rest, 'does not begin with [').

    If rest string starts with a '[', then read until we find ']'.
    If no ']' is found, return a tuple (None, rest, 'does not contain ending ]').

    Otherwise separate the string into 'before' representing characters between 
    '[' and ']' and 'after' representing characters after the ']' and then check 
    that the first character found after the ']' is a :'.  

    If there are no characters after the ']', return a tuple (before, None, None) 
    where before contains the characters between '[' and ']'.

    If there are characters after ']' other than the delimiter, return a tuple 
    (None, rest, 'characters not allowed after ending ]')

    Otherwise return a tuple (before, after, None) where before contains the 
    characters between '[' and ']' and after contains the characters after the ']:'.

    This function avoids raising Exceptions for these particular cases of 
    malformed input since they are easier to report in the calling function.

    Examples
    --------
    
    >>> read_to_bracketed(':', '[abc:def]')
    ('abc:def', None, None)
    >>> read_to_bracketed(':', '[abc]:def:ghi')
    ('abc', 'def:ghi', None)
    >>> read_to_bracketed(':', '[abc:def]:ghi:jkl')
    ('abc:def', 'ghi:jkl', None)
    >>> read_to_bracketed(':', 'abc:def:ghi:jkl')
    (None, 'abc:def:ghi:jkl', 'does not begin with [')
    >>> read_to_bracketed(':', '[abc:def:ghi:jkl')
    (None, '[abc:def:ghi:jkl', 'does not contain ending ]')
    >>> read_to_bracketed(':', '[abc]extra:def:ghi:jkl')
    (None, '[abc]extra:def:ghi:jkl', 'characters not allowed after ending ]')

    @param delimiter: the delimiter string
    @param rest:      the string to read such as '[abc:def]:ghi'
    @returns:         (before, after, reason) tuple such as ('abc:def', 'ghi', None)
    """
    if not rest.startswith('['):
        return None, rest, 'does not begin with ['
    p = rest.find(']')
    if p < 0: 
        return None, rest, 'does not contain ending ]'
    if len(rest[p+1:]) < 1:
        return rest[1:p], None, None
    if rest[p+1] != delimiter:
        return None, rest, 'characters not allowed after ending ]'
    return rest[1:p], rest[p+2:], None



def read_to_possibly_bracketed(delimiter, rest):
    """
    Behave as read_bracketed above when rest starts with a '[',
    otherwise as read_to_colon.  This is intended to support fields
    which may contain an IPv6 address, an IPv4 address or a hostname.

    Examples
    --------

    >>> read_to_possibly_bracketed(':', 'abc:def:ghi')
    ('abc', 'def:ghi', None)
    >>> read_to_possibly_bracketed(':', '[abc]:def:ghi')
    ('abc', 'def:ghi', None)
    >>> read_to_possibly_bracketed(':', '[abc:def]:ghi')
    ('abc:def', 'ghi', None)
    >>> read_to_possibly_bracketed(':', '[]:ghi')
    ('', 'ghi', None)
    >>> read_to_possibly_bracketed(':', ':ghi')
    ('', 'ghi', None)
    >>> read_to_possibly_bracketed(':', '[ghi]')
    ('ghi', None, None)
    >>> read_to_possibly_bracketed(':', '[]')
    ('', None, None)
    >>> read_to_possibly_bracketed(':', '')
    ('', None, None)

    @param delimiter: the delimiter string
    @param rest:      the string to read such as '[abc:def]:ghi'
    @returns:         (before, after, reason) tuple such as ('abc:def', 'ghi', None)
    """
    if rest.startswith('['):
        return read_to_bracketed(delimiter, rest)
    return read_to(delimiter, rest)


class LineParser:
    """
    Manage state to parse a single line, generally from a configuration
    file with fields delimited by colons.
    """

    def __init__(self, caller, filename, lineno, line):
        "Initialize"
        (self.caller, self.filename, self.lineno, self.line, self.rest, self.error) = (caller, filename, lineno, line, line, None)
        self.logger = logger
        if logging_is_verbose():
            self.logger.debug("%s:%s" % (filename, lineno))

    def ensure_more_to_process(self, name):
        "Raise an exception if we've exhausted the input line"
        if self.rest is None:
            msg = "out of values (reading %s)" % name
            raise ExceptionNoStackTraceNeeded("%s:%s:%s LINE >>%s\n%s" % (self.filename, self.lineno, self.caller, self.line, msg))

    def read_delimited_field(self, delimiter, name="next field", reader=read_to):
        """
        Attempts to read the next field in the line up to the specified delimiter
        using the specified reading method, raising any error encountered as an
        exception.  Returns the read field when successful.
        """
        self.ensure_more_to_process(name)
        value, self.rest, error = reader(delimiter, self.rest)
        if error is not None:
            msg = "%s (reading %s) >>%s" % (error, name, self.rest)
            raise ExceptionNoStackTraceNeeded("%s:%s:%s LINE >>%s\n%s" % (self.filename, self.lineno, self.caller, self.line, msg))

        if logging_is_verbose():
            self.logger.debug("  name=%-30s delimiter='%s' value=%s" % (name, delimiter, value))
        return value

    def ensure_starts_with(self, expected):
        "Returns true if line starts with expected value, or raise an exception otherwise"
        if not self.line.startswith(expected): 
            msg = "does not start with %s" % expected
            raise ExceptionNoStackTraceNeeded("%s:%s:%s LINE >>%s\n%s" % (self.filename, self.lineno, self.caller, self.line, msg))
        self.rest = self.rest[len(expected):]

    def handle_field(self, name, dst=None, delimiter=':', stripchars=None):
        """
        Attempts to read the next field up to a given delimiter.
        Names starting with '[' indicate that the field should use the bracketed parsing logic.
        If dst is not none, also assigns the value to dst[name].  
        If stripchars is not none, value is first stripped of leading and trailing stripchars.
        """
        if name[0] == '[':
            name = name.strip('[]')
            value = self.read_delimited_field(delimiter, name, read_to_possibly_bracketed)
        else:
            value = self.read_delimited_field(delimiter, name)
        if stripchars is not None:
            value = value.strip(stripchars)
        if dst is not None:
            dst[name] = value
        return value





#
# file parsing
#

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



################
# gpexpand segment file format:
#
# Form of file is hostname:address:port:dtadir:dbid:contentId:role[:replicationPort]
################

def parse_gpexpand_segment_line(filename, lineno, line):
    """
    Parse a line of the gpexpand configuration file.

    >>> parse_gpexpand_segment_line('file', 1, "localhost:[::1]:40001:/Users/ctaylor/data/p2/gpseg1:4:1:p")
    ('localhost', '::1', '40001', '/Users/ctaylor/data/p2/gpseg1', '4', '1', 'p', None)
    >>> parse_gpexpand_segment_line('file', 1, "localhost:[::1]:40001:/Users/ctaylor/data/p2/gpseg1:4:1:p:41001")
    ('localhost', '::1', '40001', '/Users/ctaylor/data/p2/gpseg1', '4', '1', 'p', '41001')
    """
    p = LineParser(caller(), filename, lineno, line)
    hostname        = p.handle_field('[host]')      # [host] indicates possible IPv6 address
    address         = p.handle_field('[address]')   # [address] indicates possible IPv6 address
    port            = p.handle_field('port')
    datadir         = p.handle_field('datadir')
    dbid            = p.handle_field('dbid')
    contentId       = p.handle_field('contentId')
    role            = p.handle_field('role')
    replicationPort = None
    if p.rest is not None:
        replicationPort = p.handle_field('replicationPort')
        if p.rest is not None:
            msg = "unexpected characters after replicationPort >>%s" % p.rest
            raise ExceptionNoStackTraceNeeded("%s:%s:%s LINE >>%s\n%s" % (filename, lineno, caller(), line, msg))
    return hostname, address, port, datadir, dbid, contentId, role, replicationPort


################
# gpaddmirrors format:
# 
# filespaceOrder=[filespace1_fsname[:filespace2_fsname:...]]
# mirror[content]=content:address:port:mir_replication_port:pri_replication_port:fselocation[:fselocation:...]  
#
################

def parse_filespace_order(filename, lineno, line):
    """
    Parse the filespaceOrder= line appearing at the beginning of the gpaddmirrors,
    gpmovemirrors and gprecoverseg configuration files.

    >>> parse_filespace_order('file', 1, "filespaceOrder=fs1:fs2:fs3")
    ['fs1', 'fs2', 'fs3']
    >>> parse_filespace_order('file', 1, "filespaceOrder=")
    []
    """
    p = LineParser(caller(), filename, lineno, line)
    p.ensure_starts_with('filespaceOrder=')
    fslist = []
    while p.rest:
        fslist.append( p.read_delimited_field(':', 'next filespace') )
    return fslist


def parse_gpaddmirrors_line(filename, lineno, line, fslist):
    """
    Parse a line in the gpaddmirrors configuration file other than the first.

    >>> line = "mirror0=0:[::1]:40001:50001:60001:/Users/ctaylor/data/p2/gpseg1"
    >>> fixed, flex = parse_gpaddmirrors_line('file', 1, line, [])
    >>> fixed["address"], fixed["contentId"], fixed["dataDirectory"]
    ('::1', '0', '/Users/ctaylor/data/p2/gpseg1')

    """
    fixed = {}
    flexible = {}
    p = LineParser(caller(), filename, lineno, line)
    p.ensure_starts_with('mirror')
    p.read_delimited_field('=', 'content id', consume_to)
    # [address] indicates possible IPv6 address
    for field in [ 'contentId', '[address]', 'port', 'replicationPort', 'primarySegmentReplicationPort', 'dataDirectory' ]:
        p.handle_field(field, fixed)
    for fsname in fslist:
        p.handle_field(fsname, flexible)
    return fixed, flexible



################
# gpmovemirrors format:
# 
# This is basically the same as the gprecoverseg format (since gpmovemirrors ultimately just
# passes the input file after validating it) but the field names are slightly different.
# 
# filespaceOrder=[filespace1_fsname[:filespace2_fsname:...]
# old_address:port:datadir new_address:port:replication_port:datadir[:fselocation:...]
#                         ^
#                         note space
################

def parse_gpmovemirrors_line(filename, lineno, line, fslist):
    """
    Parse a line in the gpmovemirrors configuration file other than the first.

    >>> line = "[::1]:40001:/Users/ctaylor/data/m2/gpseg1 [::2]:40101:50101:/Users/ctaylor/data/m2/gpseg1:/fs1"
    >>> fixed, flex = parse_gpmovemirrors_line('file', 1, line, ['fs1'])
    >>> fixed["oldAddress"], fixed["newAddress"]
    ('::1', '::2')
    >>> flex
    {'fs1': '/fs1'}

    """
    groups = len(line.split())
    if groups != 2:
        msg = "need two groups of fields delimited by a space for old and new mirror, not %d" % groups
        raise ExceptionNoStackTraceNeeded("%s:%s:%s LINE >>%s\n%s" % (filename, lineno, caller(), line, msg))
    fixed = {}
    flexible = {}
    p = LineParser(caller(), filename, lineno, line)
    p.handle_field('[oldAddress]', fixed) # [oldAddress] indicates possible IPv6 address
    p.handle_field('oldPort', fixed)
    p.handle_field('oldDataDirectory', fixed, delimiter=' ', stripchars=' \t') # MPP-15675 note stripchars here and next line
    p.handle_field('[newAddress]', fixed, stripchars=' \t') # [newAddress] indicates possible IPv6 address
    p.handle_field('newPort', fixed)
    p.handle_field('newReplicationPort', fixed)
    p.handle_field('newDataDirectory', fixed)
    for fsname in fslist:
        p.handle_field(fsname, flexible)
    if p.rest is not None:
        msg = "unexpected characters after mirror fields >>%s" % p.rest
        raise ExceptionNoStackTraceNeeded("%s:%s:%s LINE >>%s\n%s" % (filename, lineno, caller(), line, msg))
    return fixed, flexible


################
# gprecoverseg format:
#
# filespaceOrder=[filespace1_fsname[:filespace2_fsname:...]]
# failed_host_address:port:datadir [recovery_host_address:port:replication_port:datadir[:fselocation:...]]
#                                 ^
#                                 note space
# 
# filespace locations are only present at the end of the other fields when there 
# are two groups of fields separated by a space.  If there is only one group of
# fields then we assume the entire line is only three fields as below with no
# filespace locations:
#
# failed_host_address:port:datadir
################

def parse_gprecoverseg_line(filename, lineno, line, fslist):
    """
    Parse a line in the gprecoverseg configuration file other than the first.

    >>> line = "[::1]:40001:/Users/ctaylor/data/m2/gpseg1"
    >>> fixed, flex = parse_gprecoverseg_line('file', 1, line, [])
    >>> fixed["failedAddress"], fixed["failedPort"], fixed["failedDataDirectory"]
    ('::1', '40001', '/Users/ctaylor/data/m2/gpseg1')

    >>> line = "[::1]:40001:/Users/ctaylor/data/m2/gpseg1 [::2]:40101:50101:/Users/ctaylor/data/m2/gpseg1:/fs1"
    >>> fixed, flex = parse_gprecoverseg_line('file', 1, line, ['fs1'])
    >>> fixed["newAddress"], fixed["newPort"], fixed["newReplicationPort"], fixed["newDataDirectory"]
    ('::2', '40101', '50101', '/Users/ctaylor/data/m2/gpseg1')
    >>> flex
    {'fs1': '/fs1'}
    """

    groups = len(line.split())
    if groups not in [1, 2]:
        msg = "only one or two groups of fields delimited by a space, not %d" % groups
        raise ExceptionNoStackTraceNeeded("%s:%s:%s LINE >>%s\n%s" % (filename, lineno, caller(), line, msg))
    fixed = {}
    flexible = {}
    p = LineParser(caller(), filename, lineno, line)
    p.handle_field('[failedAddress]', fixed) # [failedAddress] indicates possible IPv6 address
    p.handle_field('failedPort', fixed)
    if groups == 1:
        p.handle_field('failedDataDirectory', fixed)
    else:
        p.handle_field('failedDataDirectory', fixed, delimiter=' ', stripchars=' \t') # MPP-15675 note stripchars here and next line
        p.handle_field('[newAddress]', fixed, stripchars=' \t') # [newAddress] indicates possible IPv6 address
        p.handle_field('newPort', fixed)
        p.handle_field('newReplicationPort', fixed)
        p.handle_field('newDataDirectory', fixed)
        for fsname in fslist:
            p.handle_field(fsname, flexible)
    return fixed, flexible


        
if __name__ == '__main__':
    import doctest
    doctest.testmod()


