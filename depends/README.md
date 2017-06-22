## Setting up Conan

Conan requires python 2.7 or higher to run.  To install Conan run the command:

```
	pip install conan
```

This will install all the required python packages to use Conan on your system.  Once Conan is installed, please add the Greenplum remote Conan repository.  This is where we are hosting the packages and the pre-build binaries for GPDB and associated projects.  Anonymous access is supported for package downloading and to configure access please run the command:

```
	conan remport add <REMOTE_NAME> ttps://api.bintray.com/conan/greenplum-db/gpdb-oss 
```


## Using Conan

The simplest way to run Conan is to run the command `conan install` from this directory.  It will read the conanfile.txt and copy the dependant files into the directory structure rooted here.

Conan will **not** install files into standard locations such as `/usr/local` nor will it interaction with autoconf or GNU make in any way at this time.  You will need to either install these files into an appropriate location in your files system such as `/usr/local/lib` or else update the `CFLAGS` variable to pass to configure:

```	
	CFLAGS="-I./depends/include" LDFLAGS="-L./depends/lib" ./configure
```

Afterwards you will need to either install the libraries in a standard location or update the `LD_LIBARARY_PATH` (or similar) so the gpdb binaries can find the libraries


## Contributing

We are tracking the package definitions in a separate [Conan github repository](http://github.com/greenplum-db/conan).  Instructions on how you can contribute and update dependencies can be found there.