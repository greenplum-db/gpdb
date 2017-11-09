# Configuration arguments for vcbuild.
use strict;
use warnings;

our $config = {
    asserts=>1,			# --enable-cassert
    # integer_datetimes=>1,   # --enable-integer-datetimes - on is now default
    # float4byval=>1,         # --disable-float4-byval, on by default
    # float8byval=>1,         # --disable-float8-byval, on by default
    # blocksize => 32,        # --with-blocksize, 8kB by default
    # ldap=>1,				# --with-ldap
    # wal_blocksize => 8,     # --with-wal-blocksize, 8kb by default
    # wal_segsize => 16,      # --with-wal-segsize, 16MB by default
    nls=>undef,				# --enable-nls=<path>
    tcl=>undef,				# --with-tls=<path>
    perl=>undef, 			# --with-perl
    python=>undef,			# --with-python=<path>
    krb5=>undef,			# --with-krb5=<path>
    openssl=>undef,			# --with-ssl=<path>
    uuid=>undef,			# --with-ossp-uuid
    xml=>undef,				# --with-libxml=<path>
    xslt=>undef,			# --with-libxslt=<path>
    iconv=>undef,			# (not in configure, path to iconv)
    zlib=>'c:\zlib64',			# --with-zlib=<path>  (GPDB needs zlib)
    pthread=>'c:\pthreads',  		# gpdb needs pthreads 
    curl=>'c:\zlib', 			# gpdb needs libcurl
    bz2=>'c:\pgbuild\bzlib'
    #readline=>'c:\progra~1\GnuWin32' 	# readline for windows?
};

1;
