#!/usr/bin/env python
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved. 
#
"""
  gpversion.py:

  Contains GpVersion class for handling version comparisons.
"""

# ===========================================================
import sys, os, re

# Python version 2.6.2 is expected, must be between 2.5-3.0
if sys.version_info < (2, 5, 0) or sys.version_info >= (3, 0, 0):
    sys.stderr.write("Error: %s is supported on Python versions 2.5 or greater\n" 
                     "Please upgrade python installed on this machine." 
                     % os.path.split(__file__)[-1])
    sys.exit(1)

MAIN_VERSION = [6,99,99]    # version number for main


#============================================================
class GpVersion:
    '''
    The gpversion class is an abstraction of a given Greenplum release 
    version.  It exists in order to facilitate version comparisons,
    formating, printing, etc.
    
      x = GpVersion([3,2,0,4]) => Greenplum 3.2.0.4
      x = GpVersion('3.2')     => Greenplum 3.2 dev
      x = GpVersion('3.2 build dev') => Greenplum 3.2 dev
      x = GpVersion('main')    => Greenplum main 
      x.major()                => Major release, eg "3.2"
      x.isrelease('3.2')       => Major version comparison
    '''

    """
    Past major versions of GPDB. Used for shift arithmetic
    and reasoning about separation between versions.
    """
    history = [
                '3.2',
                '3.3',
                '4.0',
                '4.1',
                '4.2',
                '4.3',
                '5',
                '6'
    ]

    #------------------------------------------------------------
    def __init__(self, version):
        '''
        The initializer for GpVersion is complicated so that it can handle
        creation via several methods:
           x = GpVersion('3.2 dev')  => via string
           x = GpVersion([3,2])  => via tuple
           y = GpVersion(x)      => copy constructor

        If the input datatype is not recognised then it is first cast to
        a string and then converted.
        '''
        try:
            self.version = None
            self.build   = None

            # Local copy that we can safely manipulate
            v = version

            # Copy constructor
            if isinstance(v, GpVersion):
                self.version = v.version
                self.build   = v.build
                return

            # if version isn't a type we recognise then convert to a string
            # first
            if not (isinstance(v, str) or
                    isinstance(v, list) or
                    isinstance(v, tuple)):
                v = str(v)

            # Convert a string into the version components.
            # 
            # There are several version formats that we anticipate receiving:
            #
            # Versions from "postgres --gp-version":
            #    ".* (Greenplum Database) <VERSION> build <BUILD>"
            #
            # Version from sql "select version()"
            #    ".* (Greenplum Database <VERSION> build <BUILD>) .*"
            #
            # Versions from python code:
            #    "<VERSION>"
            #    "<VERSION> <BUILD>"
            #
            if isinstance(v, str):
                # See if it matches one of the two the long formats
                regex = r"\(Greenplum Database\)? ([^ ]+) build ([^ )]+)"
                m = re.search(regex, v)
                if m:
                    (v, self.build) = m.groups()   # (version, build)
                
                # Remove any surplus whitespace, if present
                v = v.strip()

                # We should have either "<VERSION> <BUILD>" or "VERSION" so 
                # split on whitespace.
                vlist = v.split(' ')
                if len(vlist) == 2:
                    (v, self.build) = vlist
                elif len(vlist) == 3 and vlist[1] == 'build':
                    (v, _, self.build) = vlist
                elif len(vlist) > 2:
                    raise StandardError("too many tokens in version")
                
                # We should now just have "<VERSION>"
                if v == 'main' or v.endswith('_MAIN'):
                    self.version = MAIN_VERSION
                    if not self.build:
                        self.build = 'dev'
                    return
                
                # Check if version contains any "special build" tokens
                # e.g. "3.4.0.0_EAP1" or "3.4.filerep".  
                #
                # For special builds we use the special value for <BUILD>
                # rather than any value calculated above.
                
                # <VERSION> consists of:
                #    2 digits for major version
                #    optionally another 2 digits for minor version
                #    optionally a string specifiying a "special build", eg:
                #        
                #        we ignore the usual build version and use the special
                #        vilue for "<BUILD>" instead.
                regex = r"[0123456789.]*\d"
                m = re.search(regex, v)
                if not m:
                    raise StandardError("unable to coerce to version")
                if m.end() < len(v):
                    self.build = v[m.end()+1:]
                v = v[m.start():m.end()]
                
                # version is now just the digits, split on '.' and fall
                # into the default handling of a list argument.
                v = v.split('.')

            # Convert a tuple to a list so that extend and slicing will work 
            # nicely
            if isinstance(v, tuple):
                v = list(v)

            # Any input we received should have been 
            if not isinstance(v, list):
                raise StandardError("Internal coding error")

            # As of GPDB 5, version strings moved from four digits (4.3.X.X) to three (5.X.X)
            minlen = 2 if int(v[0]) <= 4 else 1
            maxlen = 4 if int(v[0]) <= 4 else 3
            if len(v) < minlen:
                raise StandardError("Version too short")
            elif len(v) > maxlen:
                raise StandardError("Version too long")
            elif len(v) < maxlen:
                v.extend([99,99])
            v = map(int, v)  # Convert to integers
            if v[0] <= 4:
                self.version = v[:4]
            else:
                self.version = v[:3]

            if not self.build:
                self.build = 'dev'


        # If part of the conversion process above failed, throw an error,
        except Exception as e:
            raise StandardError("Unrecognised Greenplum Version '%s' due to %s" %
                                (str(version), str(e)))

    #------------------------------------------------------------
    def __cmp__(self, other):
        '''
        One of the main reasons for this class is so that we can safely compare
        versions with each other.  This needs to be pairwise integer comparison
        of the tuples, not a string comparison, which is why we maintain the
        internal version as a list.       
        '''
        if isinstance(other, GpVersion):
            return cmp(self.version, other.version)
        else:
            return cmp(self, GpVersion(other))
    
    #------------------------------------------------------------
    def __str__(self):
        ''' 
        The other main reason for this class is that the display version is
        not the same as the internal version for main and development releases.
        '''
        if self.version == MAIN_VERSION:
            v = 'main'
        elif self.version[0] <= 4:
            if self.version[2] == 99 and self.version[3] == 99:
                v = '.'.join(map(str,self.version[:2]))
            else:
                v = '.'.join(map(str,self.version[:4]))
        else:
            if self.version[1] == 99 and self.version[2] == 99:
                v = '%d' % self.version[0]
            else:
                v = '.'.join(map(str,self.version[:3]))

        if self.build:
            return "%s build %s" % (v, self.build)
        else:
            return v
   
    def __lshift__(self, num):
        i = self.history.index(self.getVersionRelease())
        i -= num
        if i < 0 or num < 0:
            raise StandardError('invalid version shift')
        return GpVersion(self.history[i] + ".0.0")

    #------------------------------------------------------------
    def getVersionBuild(self):
        '''
        Returns the build number portion of the version.
        '''
        return self.build

    #------------------------------------------------------------
    def getVersionRelease(self):
        '''
        Returns the major (first 2 values for <= 4.3, first value for >= 5) portion of the version.
        '''
        if self.version[0] <= 4:
            return '.'.join(map(str,self.version[:2]))
        else:
            return '%d' % self.version[0]

    #------------------------------------------------------------
    def isVersionRelease(self, version):
        '''
        Returns true if the version matches a particular major release.
        '''
        other = GpVersion(version)
        return self.getVersionRelease() == other.getVersionRelease()

    #------------------------------------------------------------
    def isVersionCurrentRelease(self):
        '''
        Returns true if the version matches the current MAIN_VERSION
        '''
        return self.isVersionRelease(MAIN_VERSION)

    
