# Steps to build plpython with Anaconda

Anaconda (https://www.continuum.io/) is an excellent Python distribution for
machine learning and analytics which also provide its own package
tracking/management tools. GPDB support plpython which can make use of all
features provided by Anaconda.  Note that GPDB only supports Python 2.7 now.
Although plpython might be built with Python 3.x, other Python tools in GPDB
won't work with Python 3.x.

You can try following steps to run Anaconda with GPDB from source code.

## Install Anaconda. 
We use miniconda here.

	wget  https://repo.continuum.io/miniconda/Miniconda-latest-Linux-x86_64.sh
	chmod +x Miniconda-latest-Linux-x86_64.sh
	./Miniconda-latest-Linux-x86_64.sh

Then follow the instructions to complete the installation of Anaconda.
Assume miniconda is installed to "/PATH/TO/CONDAHOME".

IMPORTANT: Add this path to head of PATH environment, so that Anaconda become the default Python

	export PATH=/PATH/TO/CONDAHOME/bin:$PATH

## Install related tools
Following instructions are for Centos/RHEL/Fedora as example. 
	sudo yum install git
	sudo yum groupinstall "Development tools"
	sudo yum install curl-devel bzip2-devel openssl-devel perl-ExtUtils-Embed   libxml2-devel  openldap-devel  pam pam-devel  perl-devel  readline-devel 

The python-devel is not required here because it is provided by Anaconda.

## Download and build GPDB
	git clone https://github.com/greenplum-db/gpdb.git
	cd gpdb
	./configure --prefix=`pwd`/greenplumdb  --with-gssapi --with-pgport=5432 --with-libedit-preferred --with-perl --with-python --with-openssl  --with-libxml --enable-cassert --enable-debug --enable-testutils --enable-depend
	make install

Make sure "--with-python" parameter exists. Because the default Python is the Anaconda Python, It's done.

## Test link path of Python
In following example, Anaconda is installed to /home/gpadmin/miniconda.
    ldd greenplumdb/lib/postgresql/plpython.so
	...
	libpython2.7.so.1.0 => /home/gpadmin/miniconda/lib/libpython2.7.so.1.0 (0x00007f1f3c40c000)
	...

## Install Python package from Anaconda
Install numpy from Anaconda with command:

	conda install numpy

If you run a multinode cluster, make sure the install paths of Anaconda (and
GPDB) are the same on every host. gpssh can help to install Anaconda package on
each segment:

	gpssh -f hostlistfile /PATH/TO/CONDAHOME/bin/conda install numpy

## Init GPDB cluster

## Run "hello world"
You can run following example to ensure Anaconda working for you.


	DROP LANGUAGE IF EXISTS 'plpythonu' CASCADE;
	CREATE PROCEDURAL LANGUAGE 'plpythonu' HANDLER plpython_call_handler;

	CREATE OR REPLACE FUNCTION testnumpy() RETURNS float AS
	$$
	    import time,numpy
	    t1 = time.time()
	    X = numpy.arange(10000000)
	    Y = numpy.arange(10000000)
	    Z = X + Y
	    return time.time() - t1
	$$
	LANGUAGE 'plpythonu' VOLATILE;
	select testnumpy();

