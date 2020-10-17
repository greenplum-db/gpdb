#!/usr/bin/env python3
# $Id: $
"""
postgresql.conf configuration file reader

Module contents:
    readfile() - Read postgresql.conf file
    class gucdict - Container for postgresql.conf settings
    class setting - Holds one setting
    class ConfigurationError - a subclass of EnvironmentError

Example:
    import lib.pgconf as pgconf
    d = pgconf.readfile()
    port = d.int('port', 5432)
    pe = d.bool('password_encryption', False)
    sb = d.kB('shared_buffers')
    at = d.time('authentication_timeout', 'ms', 2500)
"""

import os
import os.path
import re

# Max recursion level for postgresql.conf include directives.
# The max value is 10 in the postgres code, so it's the same here.
MAX_RECURSION_LEVEL=10

def readfile(filename='postgresql.conf', defaultpath=None):
    """
    Read postgresql.conf file and put the settings into a dictionary.
    Returns the dictionary: a newly created pgconf.gucdict object.

    If filename does not specify an absolute path, it is treated as relative
    to defaultpath, or to the current working directory.
    """
    if not os.path.isabs(filename):
        if defaultpath is None:
            defaultpath = os.getcwd()
        filename = os.path.normpath(os.path.join(defaultpath, filename))

    fp = open(filename)
    try:
        dictionary = gucdict()
        dictionary.populate(fp, filename)
        return dictionary
    except Exception:
        raise
    finally:
        fp.close()


class gucdict(dict):
    """
    A container for settings from a postgresql.conf file.

    Behaves as an ordinary dictionary, with a few added methods.
    The keys of the dictionary are GUC names in lower case, and the
    values are instances of the pgconf.setting class.

    The populate() method loads the dictionary with settings from a file.

    The str(), bool(), int(), float(), kB(), and time() methods return a
    value from the dictionary, converted to internal form.
    """

    def populate(self, lines, filename='', recurLevel=0):
        '''
        Given a postgresql.conf input file (or a list of strings, or some
        iterable object yielding lines), look for lines of the form
                name[=][value][#comment]
        For each one found, construct a pgconf.setting object and put it
        into our dictionary.
        '''
        if recurLevel == MAX_RECURSION_LEVEL:
            raise Exception('could not open configuration file "%s": maximum nesting depth exceeded' % filename)
        
        linenumber = 0
        for line in lines:
            linenumber += 1
            m = _setpat.match(line)
            if m:
                name, value, pos = m.group(1), m.group(3), m.start(3)
                if name == 'include':
                    try:
                        # Remove the ' from the filename and then convert to abspath if needed.
                        incfilename = value.strip("'")
                        if not incfilename.startswith('/') and filename != '':
                            incfilename = '%s/%s' % (filename[0:filename.rfind('/')], incfilename)
                        fp = open(incfilename)
                        self.populate(fp, incfilename, recurLevel+1)
                        fp.close()
                    except IOError:
                        raise Exception('File %s included from %s:%d does not exist' % (incfilename, filename, linenumber))
                else:
                    self[name.lower()] = setting(name, value, filename, linenumber, pos)

    def str(self, name, default=None):
        """
        Return string setting, or default if absent.
        """
        v = self.get(name)
        if v:
            return v.str()
        else:
            return default

    def bool(self, name, default=None):
        """
        Return Boolean setting, or default if absent.
        """
        v = self.get(name)
        if v:
            return v.bool()
        else:
            return default

    def int(self, name, default=None):
        """
        Return integer setting, or default if absent.
        """
        v = self.get(name)
        if v:
            return v.int()
        else:
            return default

    def float(self, name, default=None):
        """
        Return floating-point setting, or default if absent.
        """
        v = self.get(name)
        if v:
            return v.float()
        else:
            return default

    def kB(self, name, default=None):
        """
        Return memory setting in units of 1024 bytes, or default if absent.
        """
        v = self.get(name)
        if v:
            return v.kB()
        else:
            return default

    def time(self, name, unit='s', default=None):
        """
        Return time setting, or default if absent.
        Specify desired unit as 'ms', 's', or 'min'.
        """
        v = self.get(name)
        if v:
            return v.time(unit)
        else:
            return default


class setting(object):
    """
    Holds a GUC setting from a postgresql.conf file.

    The str(), bool(), int(), float(), kB(), and time() methods return the
    value converted to the requested internal form.  pgconf.ConfigurationError
    is raised if the conversion fails, i.e. the value does not conform to the
    expected syntax.
    """
    def __init__(self, name, value, filename='', linenumber=0, pos=0):
        self.name = name
        self.value = value
        self.filename = filename
        self.linenumber = linenumber
        self.pos = pos       # starting offset of value within the input line

    def __repr__(self):
        return repr(self.value)

    def str(self):
        """
        Return the value as a string.
        """
        v = self.value
        if v and v.endswith("'"):
            # Single-quoted string.  Remove the opening and closing quotes.
            # Replace each escape sequence with the character it stands for.
            i = v.index("'") + 1
            v = _escapepat.sub(_escapefun, v[i:-1])
        return v

    def bool(self):
        """
        Interpret the value as a Boolean.  Returns True or False.
        """
        s = self.value
        if s:
            s = s.lower()
            n = len(s)
            if (s == '1' or
                s == 'on' or
                s == 'true'[:n] or
                s == 'yes'[:n]):
                return True
            if (s == '0' or
                s == 'off'[:n] or
                s == 'false'[:n] or
                s == 'no'[:n]):
                return False
        raise self.ConfigurationError('Boolean value should be one of: 1, 0, '
                                      'on, off, true, false, yes, no.')

    def int(self):
        """
        Interpret the value as an integer.  Returns an int or long.
        """
        try:
            return int(self.value, 0)
        except ValueError:
            raise self.ConfigurationError('Value should be integer.')

    def float(self):
        """
        Interpret the value as floating point.  Returns a float.
        """
        try:
            return float(self.value)
        except ValueError:
            raise self.ConfigurationError('Value should be floating point.')

    def kB(self):
        """
        Interpret the value as an amount of memory.  Returns an int or long,
        in units of 1024 bytes.
        """
        try:
            m = 1
            t = re.split('(kB|MB|GB)', self.value)
            if len(t) > 1:
                i = ['kB', 'MB', 'GB'].index(t[1])
                m = (1, 1024, 1024*1024)[i]
            try:
                return int(t[0], 0) * m
            except ValueError:
                pass
            return int(float(t[0]) * m)
        except (ValueError, IndexError):
            raise self.ConfigurationError('Value should be integer or float '
                                          'with optional suffix kB, MB, or GB '
                                          '(kB is default).')

    def time(self, unit='s'):
        """
        Interpret the value as a time.  Returns an int or long.
        Specify desired unit as 'ms', 's', or 'min'.
        """
        u = ['ms', 's', 'min'].index(unit)
        u = (1, 1000, 60*1000)[u]
        try:
            m = u
            t = re.split('(ms|s|min|h|d)', self.value)
            if len(t) > 1:
                i = ['ms', 's', 'min', 'h', 'd'].index(t[1])
                m = (1, 1000, 60*1000, 3600*1000, 24*3600*1000)[i]
            return int(t[0], 0) * m / u
        except (ValueError, IndexError):
            raise self.ConfigurationError('Value should be integer with '
                                          'optional suffix ms, s, min, h, or d '
                                          '(%s is default).' % unit)

    def ConfigurationError(self, msg):
        msg = '(%s = %s)  %s' % (self.name, self.value, msg)
        return ConfigurationError(msg, self.filename, self.linenumber)



class ConfigurationError(EnvironmentError):
    def __init__(self, msg, filename='', linenumber=0):
        self.msg = msg
        self.filename = filename
        self.linenumber = linenumber
        if linenumber:
            msg = '%s line %d: %s' % (filename, linenumber, msg)
        elif filename:
            msg = '%s: %s' % (filename, msg)
        EnvironmentError.__init__(self, msg)
    def __str__(self):
        return self.message


#-------------------------------- private --------------------------------

_setpat = re.compile(r"\s*(\w+)\s*(=\s*)?"           # name [=]
                     '('
                     r"[eE]?('((\\.)?[^\\']*)*')+|"  # single-quoted string or
                     r"[^\s#']*"                     # token ending at whitespace or comment
                     ')')
_escapepat = re.compile(r"''|"                   # pair of single quotes, or
                        r"\\("                   # backslash followed by
                        r"[0-7][0-7]?[0-7]?|"    # nnn (1 to 3 octal digits) or
                        r"x[0-9A-Fa-f][0-9A-Fa-f]?|" # xHH (1 or 2 hex digits) or
                        r".)")                   # one char

def _escapefun(matchobj):
    """Callback to interpret an escape sequence"""
    s = matchobj.group()
    c = s[1]
    i = "bfnrt".find(c)
    if i >= 0:
        c = "\b\f\n\r\t"[i]
    elif c == 'x':
        c = chr(int(s[2:], 16))
    elif c in '01234567':
        c = chr(int(s[1:], 8))
    return c

