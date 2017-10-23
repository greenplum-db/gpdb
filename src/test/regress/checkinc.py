#!/usr/bin/env python

import sys, os, re, subprocess

bindir = ''
if os.path.exists('../../bin/pg_config'):
    bindir = '../../bin/pg_config/'

p1 = subprocess.Popen(bindir + 'pg_config --includedir', stdout=subprocess.PIPE, shell=True)
p2 = subprocess.Popen(bindir + 'pg_config --pkgincludedir', stdout=subprocess.PIPE, shell=True)
inc    = p1.communicate()[0].strip();
pkginc = p2.communicate()[0].strip();

print "Greenplum INCLUDEDIR:    %s" % inc
print "Greenplum PKGINCLUDEDIR: %s" % pkginc
print "Checking includes..."

include = re.compile('\s*#include\s*["<](\S*)[">]')

# Hard code standard includes that are okay but we can't check via standard methods.
# Mostly port specific includes.
#
# This list should only contain system header files, or port-specific header files.
#
# Do NOT add anything from include/cdb/ to this list, as that would defeat the purpose
# of this script.
fileset = {
    'abi_mutex.h':           [],
    'alpha/builtins.h':      [],
    'crtdefs.h':             [],
    'cygwin/version.h':      [],
    'curl/curl.h':           [],
    'direct.h':              [],
    'err.h':                 [],
    'float.h':               [],
    'getopt.h':              [],
    'gssapi.h':              [],
    'gssapi/gssapi.h':       [],
    'ia64/sys/inline.h':     [],
    'intrin.h':              [],
    'io.h':                  [],
    'ioctl.h':               [],
    'kernel/OS.h':           [],
    'kernel/image.h':        [],
    'libintl.h':             [],
    'libc.h':                [],
    'mutex.h':               [],
    'ntsecapi.h':            [],
    'openssl/ssl.h':         [],
    'openssl/err.h':         [],
    'pipe.h':                [],
    'process.h':             [],
    'pthread-win32.h':       [],
    'security.h':            [],
    'solaris.h':             [],
    'SupportDefs.h':         [],
    'ssl.h':                 [],
    'stdarg.h':              [],
    'stddef.h':              [],
    'stdint.h':              [],
    'sys/isa_defs.h':        [],
    'sys/machine.h':         [],
    'sys/sdt.h':             [],
    'sys/utime.h':           [],
    'sys/atomic_op.h':       [],
    'sys/ioctl.h':           [],
    'sys/resource.h':        [],
    'sys/stat.h':            [],
    'sys/socket.h':          [],
    'sys/tas.h':             [],
    'sys/time.h':            [],
    'sys/times.h':           [],
    'sys/types.h':           [],
    'sys/un.h':              [],
    'sys/poll.h':            [],
    'unix.h':                [],
    'windows.h':             [],
    'winsock.h':             [],
    'winsock2.h':            [],
    'ws2tcpip.h':            [],
    'atomic.h':              [],
    'mbarrier.h':            [],
    'machine/sys/inline.h':  [],
    # snowball/libstemmer/header.h includes "api.h", without specifying
    # a path. Don't be alarmed by that.
    'api.h': [],
    # check src/backend/gp_libpq_fe/README
    '../../backend/gp_libpq_fe/gp-libpq-unuse.h':  []
}


# Find all the files in the shipped include directories
inc_dirs = [os.path.join(pkginc, 'server'), os.path.join(pkginc, 'internal')]
for inc_dir in inc_dirs:
    for (root, dirs, files) in os.walk(inc_dir, topdown=False):
        # For every file make a list of #includes
        for f in files:
            froot = os.path.join(root,f)
            fname = froot.replace(inc_dir+'/','')
            file = open(froot, 'r')
            fileset[fname] = []
            for line in file:
                m = include.match(line)
                if m:
                    fileset[fname].append(m.group(1))


# For all files, check to see if the list of includes is in the list
missing = False
keys = fileset.keys()
keys.sort()
for f in keys:
    for i in filter(lambda(x): None == fileset.get(x), fileset[f]):
        # There are a couple reasons an included file might not be listed:

        # 1. It might be a standard include
        if os.path.exists(os.path.join(inc,i)):
            continue

        # 2. It might be a system include
        if os.path.exists(os.path.join('/usr/include',i)):
            continue

        # 3. It might not have been well qualified
        (d,s) = os.path.split(f)
        if fileset.get(os.path.join(d,i)):
            continue

        # But if it's not one of those then it's probably a file that we have
        # omitted
        missing = True
        print '  %-30s includes missing file "%s" ' % (f, i)

if missing:
    print "Include files... FAILED"
    sys.exit(1)
else:
    print "Include files are ok" 
    sys.exit(0)
